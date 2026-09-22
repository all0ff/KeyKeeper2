#include "vault/vault_repository.hpp"

#include "security/vault_key.hpp"
#include "storage/vaultfile.hpp"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "psa/crypto.h"

#include <cstring>
#include <vector>

namespace vault::repository {

namespace {

constexpr char TAG[] = "vault.repository";

// Written/read byte-by-byte (see the little-endian helpers below), so
// this literal value is just a fixed marker, not literally the ASCII
// bytes 'K','K','V','T' in memory order -- that's enforced by
// append_u32()/Reader::read_u32() instead.
constexpr uint32_t MAGIC = 0x54564B4B;

// =============================================================================
// Encryption envelope (format v5) -- see vault_key.hpp's own file
// comment for where the AES-256 key itself comes from. Wraps the
// EXISTING plaintext v4 blob (MAGIC/VAULT_FORMAT_VERSION/entries, from
// encode_all() below, completely unchanged) inside AES-256-GCM: this
// outer layer only concerns itself with encrypt/decrypt-at-rest, not
// the entry format itself, which stays exactly as it already was.
//
// On-disk layout: ENVELOPE_MAGIC(4) | ENVELOPE_VERSION(2) | NONCE(12)
// | CIPHERTEXT(variable) | TAG(16). ENVELOPE_MAGIC+ENVELOPE_VERSION
// are passed as AEAD "additional data" (authenticated, not encrypted)
// so tampering with either is caught by the same GCM tag check as
// tampering with the ciphertext itself, even though they're stored in
// the clear (they have to be, to be readable before decryption can
// even begin). A fresh random NONCE is generated for every single
// save -- required for GCM: reusing a nonce with the same key is a
// real, serious confidentiality break, not just a formality.
//
// A file starting with ENVELOPE_MAGIC is this format. A file starting
// with the OLDER, plain MAGIC above is a pre-encryption v4 file --
// load() migrates it in place (decodes as before, then re-persists,
// which now always writes the encrypted envelope) the first time it's
// opened after this update, not a separate one-time migration step
// the person has to trigger themselves.
constexpr uint32_t ENVELOPE_MAGIC = 0x32454B4B; // distinct from MAGIC -- never valid as the old format's own first 4 bytes
constexpr uint16_t ENVELOPE_VERSION = 1;
constexpr size_t ENVELOPE_HEADER_LEN = 4 + 2; // magic + version, exactly what's passed as AAD
constexpr size_t GCM_NONCE_LEN = 12;
constexpr size_t GCM_TAG_LEN = 16;

/**
 * @brief Encrypt `plaintext` into the on-disk envelope format
 *        described above, using the current session's vault key.
 *
 * @return false if there is no vault key available right now (device
 *         not actually unlocked, or key derivation failed earlier --
 *         see vault_key.hpp) or the PSA encrypt call itself failed.
 */
bool encrypt_envelope(const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& out)
{
    if (!security::vault_key::is_set()) {
        ESP_LOGE(TAG, "encrypt_envelope: no vault key available");
        return false;
    }

    uint8_t nonce[GCM_NONCE_LEN];
    esp_fill_random(nonce, sizeof(nonce));

    uint8_t aad[ENVELOPE_HEADER_LEN];
    aad[0] = static_cast<uint8_t>(ENVELOPE_MAGIC & 0xFF);
    aad[1] = static_cast<uint8_t>((ENVELOPE_MAGIC >> 8) & 0xFF);
    aad[2] = static_cast<uint8_t>((ENVELOPE_MAGIC >> 16) & 0xFF);
    aad[3] = static_cast<uint8_t>((ENVELOPE_MAGIC >> 24) & 0xFF);
    aad[4] = static_cast<uint8_t>(ENVELOPE_VERSION & 0xFF);
    aad[5] = static_cast<uint8_t>((ENVELOPE_VERSION >> 8) & 0xFF);

    out.assign(ENVELOPE_HEADER_LEN + GCM_NONCE_LEN, 0);
    std::memcpy(out.data(), aad, ENVELOPE_HEADER_LEN);
    std::memcpy(out.data() + ENVELOPE_HEADER_LEN, nonce, GCM_NONCE_LEN);

    const size_t cipher_capacity = plaintext.size() + GCM_TAG_LEN;
    out.resize(out.size() + cipher_capacity);
    size_t cipher_len_out = 0;

    const psa_status_t status = psa_aead_encrypt(
        security::vault_key::handle(), PSA_ALG_GCM, nonce, GCM_NONCE_LEN, aad, ENVELOPE_HEADER_LEN,
        plaintext.data(), plaintext.size(), out.data() + ENVELOPE_HEADER_LEN + GCM_NONCE_LEN, cipher_capacity,
        &cipher_len_out);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_aead_encrypt failed (status %d)", static_cast<int>(status));
        return false;
    }

    out.resize(ENVELOPE_HEADER_LEN + GCM_NONCE_LEN + cipher_len_out);
    return true;
}

/**
 * @brief Decrypt an on-disk envelope (as produced by
 *        encrypt_envelope()) back into the original plaintext v4
 *        blob, using the current session's vault key.
 *
 * @return false if `in` is too short to be a valid envelope, its
 *         header doesn't match ENVELOPE_MAGIC/ENVELOPE_VERSION, there
 *         is no vault key available, or authentication fails (wrong
 *         key -- i.e. wrong PIN's derived key somehow got this far,
 *         which shouldn't normally be reachable -- or the file is
 *         corrupt/tampered).
 */
bool decrypt_envelope(const uint8_t* in, size_t in_len, std::vector<uint8_t>& out)
{
    if (in_len < ENVELOPE_HEADER_LEN + GCM_NONCE_LEN + GCM_TAG_LEN) {
        ESP_LOGE(TAG, "decrypt_envelope: file too short to be a valid envelope");
        return false;
    }

    const uint16_t version = static_cast<uint16_t>(in[4]) | (static_cast<uint16_t>(in[5]) << 8);
    if (version != ENVELOPE_VERSION) {
        ESP_LOGE(TAG, "decrypt_envelope: unknown envelope version %u", static_cast<unsigned>(version));
        return false;
    }

    if (!security::vault_key::is_set()) {
        ESP_LOGE(TAG, "decrypt_envelope: no vault key available");
        return false;
    }

    const uint8_t* aad = in; // the header itself IS the AAD, verbatim
    const uint8_t* nonce = in + ENVELOPE_HEADER_LEN;
    const uint8_t* ciphertext = in + ENVELOPE_HEADER_LEN + GCM_NONCE_LEN;
    const size_t ciphertext_len = in_len - ENVELOPE_HEADER_LEN - GCM_NONCE_LEN;

    out.assign(ciphertext_len, 0); // plaintext is always shorter than ciphertext (by GCM_TAG_LEN) -- generous capacity
    size_t plain_len_out = 0;

    const psa_status_t status = psa_aead_decrypt(security::vault_key::handle(), PSA_ALG_GCM, nonce, GCM_NONCE_LEN,
                                                  aad, ENVELOPE_HEADER_LEN, ciphertext, ciphertext_len, out.data(),
                                                  out.size(), &plain_len_out);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_aead_decrypt failed (status %d) -- wrong key or corrupt/tampered file",
                 static_cast<int>(status));
        return false;
    }

    out.resize(plain_len_out);
    return true;
}

bool initialized = false;
bool loaded = false;
std::vector<VaultEntry> entries;
uint32_t next_id = 1;

uint32_t now_s()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1'000'000);
}

void secure_clear_string(std::string& value)
{
    volatile char* data = value.empty() ? nullptr : value.data();
    for (size_t i = 0; data != nullptr && i < value.size(); ++i) {
        data[i] = '\0';
    }
    value.clear();
}

void secure_clear_bytes(std::vector<uint8_t>& value)
{
    volatile uint8_t* data = value.empty() ? nullptr : value.data();
    for (size_t i = 0; data != nullptr && i < value.size(); ++i) {
        data[i] = 0;
    }
    value.clear();
}

void secure_clear_entry(VaultEntry& entry)
{
    secure_clear_string(entry.login);
    secure_clear_string(entry.password);
    secure_clear_string(entry.url);
    secure_clear_string(entry.notes);
    secure_clear_string(entry.totp_secret);
    secure_clear_string(entry.category);
    entry.favorite = false;
    entry.id = INVALID_ID;
    entry.created_at = 0;
    entry.updated_at = 0;
}

void clear_entries()
{
    for (VaultEntry& entry : entries) {
        secure_clear_entry(entry);
    }
    entries.clear();
    entries.shrink_to_fit();
    next_id = 1;
}

// ---------------------------------------------------------------
// Little-endian primitive helpers. Written explicitly byte-by-byte
// rather than reinterpret_cast, so the on-disk format doesn't depend
// on this MCU's native endianness (or that of some future PC-side
// backup-reading tool).
// ---------------------------------------------------------------

void append_u8(std::vector<uint8_t>& buf, uint8_t v) { buf.push_back(v); }

void append_u16(std::vector<uint8_t>& buf, uint16_t v)
{
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void append_u32(std::vector<uint8_t>& buf, uint32_t v)
{
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void append_str(std::vector<uint8_t>& buf, const std::string& s)
{
    buf.insert(buf.end(), s.begin(), s.end());
}

// ---------------------------------------------------------------
// TLV field types for one entry's fields section.
// ---------------------------------------------------------------

enum FieldType : uint8_t
{
    FIELD_ID = 0,
    FIELD_LOGIN = 1,
    FIELD_PASSWORD = 2,
    FIELD_URL = 3,
    FIELD_NOTES = 4,
    FIELD_TOTP_SECRET = 5,
    FIELD_CREATED_AT = 6,
    FIELD_UPDATED_AT = 7,
    // Added in format v2 -- see vault_repository.hpp's
    // VAULT_FORMAT_VERSION comment. A v1 file simply never wrote
    // these, which decode_entry() below already handles correctly
    // (the field's absence leaves category/favorite at their
    // default-constructed values: empty string, false).
    FIELD_CATEGORY = 8,
    FIELD_FAVORITE = 9,
    // Added in format v3 -- same forward-compat story as v2's fields
    // above (an older firmware's decode_entry() hits the `default:`
    // case below and skips it, rather than choking on an unknown
    // field type).
    FIELD_RECOVERY_CODES = 10,
    // Added in format v4 -- same forward-compat story as v3's field
    // above.
    FIELD_SEED_PHRASE = 11,
};

void write_field_u8(std::vector<uint8_t>& buf, FieldType type, uint8_t value)
{
    append_u8(buf, static_cast<uint8_t>(type));
    append_u16(buf, 1);
    append_u8(buf, value);
}

void write_field_u32(std::vector<uint8_t>& buf, FieldType type, uint32_t value)
{
    append_u8(buf, static_cast<uint8_t>(type));
    append_u16(buf, 4);
    append_u32(buf, value);
}

void write_field_str(std::vector<uint8_t>& buf, FieldType type, const std::string& value)
{
    append_u8(buf, static_cast<uint8_t>(type));
    append_u16(buf, static_cast<uint16_t>(value.size()));
    append_str(buf, value);
}

/**
 * @brief FIELD_RECOVERY_CODES's payload is itself a small nested
 *        structure (a list, unlike every other field so far, which
 *        are all flat scalars/strings): a u16 count, then for each
 *        code a u16 length + the code's own bytes + a u8 used flag.
 *        Bounds-checked the same way as everything else when read
 *        back -- see decode_entry()'s FIELD_RECOVERY_CODES case,
 *        which just calls the SAME Reader sequentially rather than
 *        needing a separate sub-reader.
 */
void write_field_recovery_codes(std::vector<uint8_t>& buf, const std::vector<vault::RecoveryCode>& codes)
{
    std::vector<uint8_t> payload;
    append_u16(payload, static_cast<uint16_t>(codes.size()));
    for (const vault::RecoveryCode& rc : codes) {
        append_u16(payload, static_cast<uint16_t>(rc.code.size()));
        append_str(payload, rc.code);
        payload.push_back(rc.used ? 1 : 0);
    }

    append_u8(buf, static_cast<uint8_t>(FIELD_RECOVERY_CODES));
    append_u16(buf, static_cast<uint16_t>(payload.size()));
    buf.insert(buf.end(), payload.begin(), payload.end());
}

/**
 * @brief FIELD_SEED_PHRASE's payload: u16 word count, then for each
 *        word (IN ORDER -- see vault::VaultEntry::seed_phrase's own
 *        comment on why order matters here unlike recovery codes) a
 *        u8 length + the word's own bytes. No per-word flag needed
 *        (unlike recovery codes' used bit) -- a seed phrase word
 *        doesn't have a used/unused state.
 */
void write_field_seed_phrase(std::vector<uint8_t>& buf, const std::vector<std::string>& words)
{
    std::vector<uint8_t> payload;
    append_u16(payload, static_cast<uint16_t>(words.size()));
    for (const std::string& w : words) {
        append_u8(payload, static_cast<uint8_t>(w.size()));
        append_str(payload, w);
    }

    append_u8(buf, static_cast<uint8_t>(FIELD_SEED_PHRASE));
    append_u16(buf, static_cast<uint16_t>(payload.size()));
    buf.insert(buf.end(), payload.begin(), payload.end());
}

std::vector<uint8_t> encode_entry(const VaultEntry& e)
{
    std::vector<uint8_t> fields;
    write_field_u32(fields, FIELD_ID, e.id);
    write_field_str(fields, FIELD_LOGIN, e.login);
    write_field_str(fields, FIELD_PASSWORD, e.password);
    write_field_str(fields, FIELD_URL, e.url);
    write_field_str(fields, FIELD_NOTES, e.notes);
    write_field_str(fields, FIELD_TOTP_SECRET, e.totp_secret);
    write_field_str(fields, FIELD_CATEGORY, e.category);
    write_field_u8(fields, FIELD_FAVORITE, e.favorite ? 1 : 0);
    write_field_recovery_codes(fields, e.recovery_codes);
    write_field_seed_phrase(fields, e.seed_phrase);
    write_field_u32(fields, FIELD_CREATED_AT, e.created_at);
    write_field_u32(fields, FIELD_UPDATED_AT, e.updated_at);

    std::vector<uint8_t> record;
    append_u32(record, static_cast<uint32_t>(fields.size()));
    record.insert(record.end(), fields.begin(), fields.end());
    return record;
}

// ---------------------------------------------------------------
// Bounds-checked cursor reader. vault.db could in principle be
// corrupt (bad flash, a hand-edited file, a future bug) despite the
// atomic write in storage::vaultfile -- every read here is checked
// against the buffer end, nothing trusts the declared lengths blindly.
// ---------------------------------------------------------------

class Reader
{
public:
    Reader(const uint8_t* data, size_t len) : data_(data), len_(len) {}

    bool read_u8(uint8_t& out) { return read_bytes(&out, 1); }

    bool read_u16(uint16_t& out)
    {
        uint8_t b[2];
        if (!read_bytes(b, 2)) return false;
        out = static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
        return true;
    }

    bool read_u32(uint32_t& out)
    {
        uint8_t b[4];
        if (!read_bytes(b, 4)) return false;
        out = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
              (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        return true;
    }

    bool read_str(size_t n, std::string& out)
    {
        if (pos_ + n > len_) return false;
        out.assign(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n;
        return true;
    }

    bool skip(size_t n)
    {
        if (pos_ + n > len_) return false;
        pos_ += n;
        return true;
    }

    size_t remaining() const { return len_ - pos_; }
    size_t position() const { return pos_; }

private:
    bool read_bytes(uint8_t* out, size_t n)
    {
        if (pos_ + n > len_) return false;
        memcpy(out, data_ + pos_, n);
        pos_ += n;
        return true;
    }

    const uint8_t* data_;
    size_t len_;
    size_t pos_ = 0;
};

bool decode_entry(const uint8_t* data, size_t len, VaultEntry& out)
{
    Reader r(data, len);

    while (r.remaining() > 0) {
        uint8_t type = 0;
        uint16_t field_len = 0;
        if (!r.read_u8(type) || !r.read_u16(field_len)) {
            ESP_LOGE(TAG, "decode_entry: truncated field header");
            return false;
        }

        switch (static_cast<FieldType>(type)) {
            case FIELD_ID: {
                uint32_t v;
                if (field_len != 4 || !r.read_u32(v)) return false;
                out.id = v;
                break;
            }
            case FIELD_LOGIN:
                if (!r.read_str(field_len, out.login)) return false;
                break;
            case FIELD_PASSWORD:
                if (!r.read_str(field_len, out.password)) return false;
                break;
            case FIELD_URL:
                if (!r.read_str(field_len, out.url)) return false;
                break;
            case FIELD_NOTES:
                if (!r.read_str(field_len, out.notes)) return false;
                break;
            case FIELD_TOTP_SECRET:
                if (!r.read_str(field_len, out.totp_secret)) return false;
                break;
            case FIELD_CATEGORY:
                if (!r.read_str(field_len, out.category)) return false;
                break;
            case FIELD_FAVORITE: {
                uint8_t v = 0;
                if (field_len != 1 || !r.read_u8(v)) return false;
                out.favorite = (v != 0);
                break;
            }
            case FIELD_RECOVERY_CODES: {
                uint16_t count = 0;
                if (!r.read_u16(count)) return false;
                out.recovery_codes.clear();
                out.recovery_codes.reserve(count);
                for (uint16_t i = 0; i < count; ++i) {
                    uint16_t code_len = 0;
                    if (!r.read_u16(code_len)) return false;
                    std::string code_str;
                    if (!r.read_str(code_len, code_str)) return false;
                    uint8_t used = 0;
                    if (!r.read_u8(used)) return false;
                    out.recovery_codes.push_back(vault::RecoveryCode{std::move(code_str), used != 0});
                }
                break;
            }
            case FIELD_SEED_PHRASE: {
                uint16_t count = 0;
                if (!r.read_u16(count)) return false;
                out.seed_phrase.clear();
                out.seed_phrase.reserve(count);
                for (uint16_t i = 0; i < count; ++i) {
                    uint8_t word_len = 0;
                    if (!r.read_u8(word_len)) return false;
                    std::string word;
                    if (!r.read_str(word_len, word)) return false;
                    out.seed_phrase.push_back(std::move(word));
                }
                break;
            }
            case FIELD_CREATED_AT: {
                uint32_t v;
                if (field_len != 4 || !r.read_u32(v)) return false;
                out.created_at = v;
                break;
            }
            case FIELD_UPDATED_AT: {
                uint32_t v;
                if (field_len != 4 || !r.read_u32(v)) return false;
                out.updated_at = v;
                break;
            }
            default:
                // Unknown field, e.g. written by a future format
                // version -- skip it rather than fail the whole entry.
                if (!r.skip(field_len)) return false;
                break;
        }
    }

    return true;
}

bool decode_all(const uint8_t* data, size_t len, std::vector<VaultEntry>& out)
{
    Reader r(data, len);

    uint32_t magic = 0;
    uint16_t version = 0;
    uint16_t reserved = 0;
    uint32_t count = 0;

    if (!r.read_u32(magic) || magic != MAGIC) {
        ESP_LOGE(TAG, "decode_all: bad magic (not a KeyKeeper2 vault.db?)");
        return false;
    }
    if (!r.read_u16(version)) {
        return false;
    }
    if (version < 1 || version > VAULT_FORMAT_VERSION) {
        ESP_LOGE(TAG, "decode_all: unsupported format version %u (this firmware supports v1..v%u)",
                 version, VAULT_FORMAT_VERSION);
        return false;
    }
    if (version < VAULT_FORMAT_VERSION) {
        // Nothing else to do here -- decode_entry() above already
        // treats fields introduced after this file's version as
        // simply absent, leaving them at their default-constructed
        // values (e.g. category/favorite added in v2). The file gets
        // rewritten at the current version next time anything in it
        // is saved (add/update/remove all call persist()).
        ESP_LOGI(TAG, "decode_all: loading older vault.db format v%u (current v%u)",
                 static_cast<unsigned>(version), static_cast<unsigned>(VAULT_FORMAT_VERSION));
    }
    if (!r.read_u16(reserved) || !r.read_u32(count)) {
        return false;
    }

    out.clear();
    out.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t entry_len = 0;
        if (!r.read_u32(entry_len)) {
            ESP_LOGE(TAG, "decode_all: truncated entry length at record %u", i);
            return false;
        }
        if (entry_len > r.remaining()) {
            ESP_LOGE(TAG, "decode_all: entry %u length exceeds file", i);
            return false;
        }

        VaultEntry entry;
        if (!decode_entry(data + r.position(), entry_len, entry)) {
            ESP_LOGE(TAG, "decode_all: failed to decode entry %u", i);
            return false;
        }
        r.skip(entry_len);

        out.push_back(std::move(entry));
    }

    return true;
}

std::vector<uint8_t> encode_all(const std::vector<VaultEntry>& in)
{
    std::vector<uint8_t> buf;
    append_u32(buf, MAGIC);
    append_u16(buf, VAULT_FORMAT_VERSION);
    append_u16(buf, 0); // reserved
    append_u32(buf, static_cast<uint32_t>(in.size()));

    for (const VaultEntry& e : in) {
        const std::vector<uint8_t> record = encode_entry(e);
        buf.insert(buf.end(), record.begin(), record.end());
    }

    return buf;
}

bool persist()
{
    std::vector<uint8_t> plaintext = encode_all(entries);

    std::vector<uint8_t> envelope;
    const bool encrypted = encrypt_envelope(plaintext, envelope);
    secure_clear_bytes(plaintext);

    if (!encrypted) {
        ESP_LOGE(TAG, "persist: encrypt_envelope failed -- vault.db NOT written");
        return false;
    }

    const bool written = storage::vaultfile::write_all(envelope.data(), envelope.size());
    secure_clear_bytes(envelope);

    if (!written) {
        ESP_LOGE(TAG, "persist: storage::vaultfile::write_all failed");
        return false;
    }
    return true;
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    clear_entries();
    loaded = false;
    initialized = true;

    ESP_LOGI(TAG, "Repository initialized without loading vault.db");
    return true;
}

bool load()
{
    if (!initialized) {
        return false;
    }

    if (loaded) {
        return true;
    }

    clear_entries();

    if (!storage::vaultfile::exists()) {
        ESP_LOGI(TAG, "No vault.db yet -- starting with an empty vault");
        loaded = true;
        return true;
    }

    const size_t file_size = storage::vaultfile::size();
    std::vector<uint8_t> buf(file_size);
    size_t read_size = file_size;

    if (!storage::vaultfile::read_all(buf.data(), read_size)) {
        ESP_LOGE(TAG, "Failed to read vault.db");
        secure_clear_bytes(buf);
        return false;
    }

    // Format detection: the first 4 bytes distinguish the encrypted
    // envelope (ENVELOPE_MAGIC) from an older, pre-encryption plain
    // v4 file (the plain MAGIC) -- see ENVELOPE_MAGIC's own comment
    // for why these two values can never collide. A file starting
    // with anything else is corrupt/unrecognized either way.
    uint32_t file_magic = 0;
    if (read_size >= 4) {
        file_magic = static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
                     (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
    }

    std::vector<uint8_t> plaintext;
    bool needs_migration = false;

    if (file_magic == ENVELOPE_MAGIC) {
        if (!decrypt_envelope(buf.data(), read_size, plaintext)) {
            ESP_LOGE(TAG, "vault.db could not be decrypted -- wrong PIN's key, or the file is corrupt/tampered");
            secure_clear_bytes(buf);
            return false;
        }
    } else if (file_magic == MAGIC) {
        // Pre-encryption vault.db -- migrate it to the encrypted
        // envelope automatically, right now, rather than asking the
        // person to do anything. plaintext IS just buf here (already
        // decoded-ready), not a separate decrypt step.
        ESP_LOGW(TAG, "vault.db predates encryption -- migrating to the encrypted format now");
        plaintext = std::move(buf);
        needs_migration = true;
    } else {
        ESP_LOGE(TAG, "vault.db has an unrecognized header -- corrupt or not a KeyKeeper2 vault file");
        secure_clear_bytes(buf);
        return false;
    }

    if (!decode_all(plaintext.data(), plaintext.size(), entries)) {
        ESP_LOGE(TAG, "vault.db is corrupt or unreadable");
        secure_clear_bytes(plaintext);
        secure_clear_bytes(buf);
        clear_entries();
        return false;
    }

    secure_clear_bytes(plaintext);
    secure_clear_bytes(buf);

    for (const VaultEntry& e : entries) {
        if (e.id >= next_id) {
            next_id = e.id + 1;
        }
    }

    loaded = true;
    ESP_LOGI(TAG, "Loaded %u entries from vault.db", static_cast<unsigned>(entries.size()));

    if (needs_migration) {
        // Re-persist NOW, while the key from this same unlock is
        // still available -- persist() always writes the encrypted
        // envelope, so this one call is the entire migration. Not a
        // hard failure if it doesn't work (e.g. no vault key somehow)
        // -- the in-memory vault this session is using is correct
        // either way; the NEXT successful save (any normal edit)
        // would just retry the same migration.
        if (!persist()) {
            ESP_LOGE(TAG, "Failed to migrate vault.db to the encrypted format -- will retry on the next save");
        } else {
            ESP_LOGI(TAG, "vault.db migrated to the encrypted format");
        }
    }

    return true;
}

void clear()
{
    if (!initialized) {
        return;
    }

    clear_entries();
    loaded = false;
    ESP_LOGI(TAG, "In-memory vault cleared");
}

bool is_initialized()
{
    return initialized;
}

bool is_loaded()
{
    return initialized && loaded;
}

bool persist_now()
{
    if (!initialized || !loaded) {
        return false;
    }
    return persist();
}

size_t entry_count()
{
    return loaded ? entries.size() : 0;
}

size_t list(VaultEntry* out, size_t max_count, size_t offset)
{
    if (!initialized || !loaded || out == nullptr || offset >= entries.size()) {
        return 0;
    }

    size_t copied = 0;
    for (size_t i = offset; i < entries.size() && copied < max_count; ++i, ++copied) {
        out[copied] = entries[i];
    }
    return copied;
}

bool get(uint32_t id, VaultEntry& out)
{
    if (!initialized || !loaded) {
        return false;
    }
    for (const VaultEntry& e : entries) {
        if (e.id == id) {
            out = e;
            return true;
        }
    }
    return false;
}

std::vector<uint8_t> export_plaintext()
{
    if (!initialized || !loaded) {
        return {};
    }
    return encode_all(entries);
}

uint32_t add(VaultEntry entry)
{
    if (!initialized || !loaded || !validate(entry)) {
        return INVALID_ID;
    }

    entry.id = next_id++;
    entry.created_at = now_s();
    entry.updated_at = entry.created_at;

    entries.push_back(entry);

    if (!persist()) {
        entries.pop_back(); // keep the in-memory cache consistent with disk
        return INVALID_ID;
    }

    return entry.id;
}

bool update(const VaultEntry& entry)
{
    if (!initialized || !loaded || !validate(entry) || entry.id == INVALID_ID) {
        return false;
    }

    for (VaultEntry& e : entries) {
        if (e.id == entry.id) {
            const VaultEntry previous = e;

            e = entry;
            e.created_at = previous.created_at; // never changed by update()
            e.updated_at = now_s();

            if (!persist()) {
                e = previous;
                return false;
            }
            return true;
        }
    }

    return false;
}

bool remove(uint32_t id)
{
    if (!initialized || !loaded) {
        return false;
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].id == id) {
            const VaultEntry removed_entry = entries[i];
            entries.erase(entries.begin() + i);

            if (!persist()) {
                entries.insert(entries.begin() + i, removed_entry);
                return false;
            }
            return true;
        }
    }

    return false;
}

} // namespace vault::repository
