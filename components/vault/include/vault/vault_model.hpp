#pragma once

#include <cstdint>
#include <string>

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
// scope note). totp_secret sitting here in plaintext is exactly as
// exposed to a physical flash dump as password is -- this is not a
// separate, smaller risk.
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

/// Never a valid entry id -- used as a "not found" / "not yet saved"
/// sentinel.
inline constexpr uint32_t INVALID_ID = 0;

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

} // namespace vault
