#pragma once

#include <cstdint>
#include <string>
#include <vector>

// =============================================================================
// vault -- vault_model.hpp
//
// VaultEntry: one account record. Fields per the build plan (Account/
// Login/Password/URL/Notes/Metadata) plus totp_secret, per
// docs/STORAGE.md ("vault.db содержит ... OTP"). category/favorite
// added later (format v2) for docs/GUI.md's Vault/Account/Search/
// Favorites/Categories screens, which name these fields explicitly.
//
// IMPORTANT: totp_secret is stored here, but code GENERATION from it
// lives in components/totp, not here -- this component only ever
// holds the Base32 secret string, same as it holds the password.
// components/totp additionally needs components/rtc_time (NTP over
// Wi-Fi Station, no RTC chip on this board -- see that component's
// own file comment) to actually produce a code.
//
// IMPORTANT: vault.db is not encrypted (see components/security's
// scope note). totp_secret and seed_phrase sitting here in plaintext
// are exactly as exposed to a physical flash dump as password is --
// this is not a separate, smaller risk. For seed_phrase specifically
// (see that field's own comment) there is no cushion at all: it is
// direct, unmediated control of real funds, not a password to some
// account.
// =============================================================================

namespace vault {

// Generous but bounded -- keeps each field's TLV length in
// vault_repository.cpp representable in a uint16_t with room to
// spare, and keeps one malformed/huge field from being able to
// exhaust RAM while loading vault.db.
inline constexpr size_t MAX_LOGIN_LEN = 128;
inline constexpr size_t MAX_PASSWORD_LEN = 128;
inline constexpr size_t MAX_URL_LEN = 256;
inline constexpr size_t MAX_NOTES_LEN = 512;
inline constexpr size_t MAX_TOTP_SECRET_LEN = 128;
inline constexpr size_t MAX_CATEGORY_LEN = 64;

/// "6458f-49d3c" style -- 10 lowercase hex characters plus one
/// separating dash. See generate_recovery_codes()'s own comment for
/// the exact shape.
inline constexpr size_t MAX_RECOVERY_CODE_LEN = 32;

/// Matches the "10-20 lines" the project owner asked for -- a cap,
/// not a fixed count; generate_recovery_codes() takes its own count
/// argument within this bound.
inline constexpr size_t MAX_RECOVERY_CODES = 20;

/// BIP-39 words are 3-8 characters (english.txt); generous headroom
/// kept anyway rather than hardcoding exactly 8, in case a future
/// non-English wordlist is ever added to vault::bip39.
inline constexpr size_t MAX_SEED_WORD_LEN = 16;

/// BIP-39's longest defined length -- see
/// vault::bip39::validate_seed_phrase().
inline constexpr size_t MAX_SEED_PHRASE_WORDS = 24;

/// Never a valid entry id -- used as a "not found" / "not yet saved"
/// sentinel.
inline constexpr uint32_t INVALID_ID = 0;

/// One single-use recovery/backup code -- the "6458f-49d3c" style
/// list some services (GitHub 2FA, crypto wallets, ...) give you to
/// regain access if you lose your normal login method. Not the same
/// thing as totp_secret: a TOTP secret regenerates a new code every
/// 30 seconds from one shared secret, while these are a FIXED list of
/// individually one-time-use codes, each crossed off once spent.
/// Added in format v3 -- see vault_repository.hpp.
struct RecoveryCode
{
    std::string code;
    bool used = false;
};

struct VaultEntry
{
    uint32_t id = INVALID_ID;

    std::string login;
    std::string password;
    std::string url;
    std::string notes;

    /// Storage only -- see the file-level comment. Empty means "no
    /// TOTP configured for this entry".
    std::string totp_secret;

    /// Free-text category, e.g. "Work", "Banking". Empty means
    /// uncategorized. Added in format v2 -- see vault_repository.hpp
    /// for how older vault.db files (no category data at all) load.
    std::string category;

    /// Added in format v2, same as category. Defaults to false for
    /// entries from an older vault.db that never had this field.
    bool favorite = false;

    /// Empty means none generated for this entry. Added in format v3
    /// -- see RecoveryCode's own comment, and
    /// vault::generate_recovery_codes() for how these get created.
    std::vector<RecoveryCode> recovery_codes;

    /// A crypto wallet's mnemonic recovery phrase (BIP-39), one
    /// lowercase word per element, IN ORDER -- word position matters,
    /// unlike recovery_codes above (which is an unordered set of
    /// individually spendable codes). ONE phrase per entry (the
    /// project owner's own stated scope -- multiple wallets means
    /// multiple entries, not multiple phrases on one). Empty means
    /// none set. See vault::bip39::validate_seed_phrase() for the
    /// length (12/15/18/21/24 words) and wordlist checks this must
    /// pass before being accepted -- and that function's own comment
    /// for what it deliberately does NOT check (the BIP-39 checksum).
    /// Added in format v4.
    ///
    /// EXTRA SENSITIVE, more so than password or totp_secret: this is
    /// literal, unmediated control of whatever crypto funds the
    /// wallet holds, not a password to some account -- the SAME
    /// "vault.db is not encrypted at rest" limitation from this
    /// file's own top comment applies here with NO cushion at all.
    std::vector<std::string> seed_phrase;

    /// Unix epoch seconds, best-effort (see components/vault/README.md
    /// -- there is currently no trustworthy time source, so these may
    /// simply be 0 or drift). Not relied on for anything security-
    /// critical, purely informational ("last updated" in a GUI list).
    uint32_t created_at = 0;
    uint32_t updated_at = 0;
};

/**
 * @brief Check every field of entry against the MAX_*_LEN limits
 *        above.
 *
 * VaultRepository calls this before persisting -- an entry that fails
 * validation is never written to vault.db.
 */
bool validate(const VaultEntry& entry);

/**
 * @brief Generate a fresh set of single-use recovery codes, e.g.
 *        "6458f-49d3c" -- 10 lowercase hex characters (esp_random(),
 *        same non-cryptographic-but-hardware-backed source
 *        components/password_gen already uses) split into two groups
 *        of 5 by a dash, purely for readability -- no semantic
 *        meaning to the split point.
 *
 * REPLACES whatever recovery codes the entry already had -- like
 * regenerating recovery codes on GitHub or a crypto wallet, the old
 * ones are invalidated, not added to.
 *
 * @param count Clamped to [1, MAX_RECOVERY_CODES].
 */
std::vector<RecoveryCode> generate_recovery_codes(size_t count);

} // namespace vault
