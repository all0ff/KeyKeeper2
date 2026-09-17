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

/// Sized for whatever format a real service's own codes come in --
/// not any particular shape, since every service's are different
/// (see RecoveryCode's own comment on why this device doesn't
/// generate its own).
inline constexpr size_t MAX_RECOVERY_CODE_LEN = 32;

/// A cap, not a fixed count -- matches the "10-20 lines" the project
/// owner originally asked for as a reasonable upper bound for however
/// many codes a real service's own set turns out to be.
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

/// One single-use recovery/backup code -- the kind some services
/// (GitHub 2FA, a bank, an exchange, ...) give you to regain access
/// if you lose your normal login method. Not the same thing as
/// totp_secret: a TOTP secret regenerates a new code every 30 seconds
/// from one shared secret, while these are a FIXED list of
/// individually one-time-use codes, each crossed off once spent.
///
/// ALWAYS comes FROM that outside service, never invented here --
/// see generate_recovery_codes()'s own comment (now unused) for why
/// an on-device-generated code would be actively harmful, not
/// merely pointless. `code` is stored exactly as the person pasted
/// or imported it -- no assumed format, since every service's own
/// codes look different from every other's.
///
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

    /// Empty means none stored for this entry. See RecoveryCode's
    /// own comment -- these come from an outside service, not
    /// generated here. Added in format v3.
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
 * @brief Generates a set of RANDOM, MEANINGLESS "recovery codes".
 *
 * NOT CALLED ANYWHERE -- kept only as a record of a real mistake, not
 * as a utility. Recovery codes only mean anything in relation to the
 * OUTSIDE service (GitHub, a bank, an exchange, ...) that will
 * eventually be asked to accept one back -- only that service can
 * generate codes it will actually recognize. A device-invented code
 * looks exactly like a real one and is worse than storing nothing:
 * it creates false confidence that the account is backed up when it
 * isn't. Caught by the project owner after this was built and
 * briefly wired into the web UI's own "Generate" button -- see
 * web_vault_routes.cpp's handle_set_recovery_codes() (PUT, stores
 * exactly what the person pasted or imported) for what replaced it.
 */
std::vector<RecoveryCode> generate_recovery_codes(size_t count);

} // namespace vault
