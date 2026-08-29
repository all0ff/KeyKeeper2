#pragma once

#include <cstdint>
#include <string>

// =============================================================================
// vault -- vault_model.hpp
//
// VaultEntry: one account record. Fields per the build plan (Account/
// Login/Password/URL/Notes/Metadata) plus totp_secret, per
// docs/STORAGE.md ("vault.db содержит ... OTP").
//
// IMPORTANT: totp_secret is stored, but nothing in this component
// generates a TOTP code from it yet -- that needs a trustworthy time
// source (no RTC chip, no NTP/Wi-Fi built yet), deliberately deferred.
// See components/vault/README.md.
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
