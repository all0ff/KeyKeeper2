#include "vault/vault_repository.hpp"

#include "storage/vaultfile.hpp"

#include "esp_log.h"
#include "esp_timer.h"

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
    std::vector<uint8_t> buf = encode_all(entries);
    const bool written = storage::vaultfile::write_all(buf.data(), buf.size());
    secure_clear_bytes(buf);

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

    if (!decode_all(buf.data(), read_size, entries)) {
        ESP_LOGE(TAG, "vault.db is corrupt or unreadable");
        secure_clear_bytes(buf);
        clear_entries();
        return false;
    }

    secure_clear_bytes(buf);

    for (const VaultEntry& e : entries) {
        if (e.id >= next_id) {
            next_id = e.id + 1;
        }
    }

    loaded = true;
    ESP_LOGI(TAG, "Loaded %u entries from vault.db", static_cast<unsigned>(entries.size()));
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
