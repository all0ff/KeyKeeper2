#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// =============================================================================
// totp -- RFC 6238 (TOTP) / RFC 4226 (HOTP) code generation.
//
// Needs a trustworthy clock -- see rtc_time.hpp for why this board
// has none by default (no battery-backed RTC chip, NTP-over-Wi-Fi
// only) and what "trustworthy" means here. generate() checks
// rtc_time::is_synced() itself and fails cleanly (returns false) if
// it isn't, rather than silently producing a code for the wrong
// moment.
//
// Algorithm: standard TOTP with SHA-1 (the near-universal default
// every authenticator app and 2FA setup QR code assumes unless it
// says otherwise -- vault::VaultEntry::totp_secret has no separate
// "algorithm"/"digits"/"period" fields, so this isn't configurable
// per entry), 30-second time step, 6-digit codes, dynamic truncation
// per RFC 4226 section 5.3. HMAC-SHA-1 via PSA Crypto's
// psa_mac_compute() -- same PSA Crypto stack (psa_crypto_init(),
// already-initialized elsewhere) this project already relies on for
// PIN hashing (security::pin's PBKDF2), just a different algorithm.
//
// totp_secret is expected Base32-encoded (RFC 4648, the standard
// shape a TOTP QR code or "manual entry" string comes in, e.g.
// "JBSWY3DPEHPK3PXP") -- decoded internally, not stored decoded.
// =============================================================================

namespace totp {

/**
 * @brief Generate the CURRENT 6-digit TOTP code for a Base32-encoded
 *        secret.
 *
 * @param secret_base32 Case-insensitive; '=' padding and internal
 *                       spaces/dashes are tolerated and ignored (some
 *                       apps format manual-entry keys with either).
 * @param out_code       Receives a null-terminated 6-digit code
 *                        ("000000" to "999999").
 * @param out_code_size  Must be >= 7.
 *
 * @return false if: rtc_time::is_synced() is false (see that
 *         function's own comment), secret_base32 is empty or contains
 *         characters outside the Base32 alphabet, or out_code_size is
 *         too small. out_code is left untouched on failure.
 */
bool generate(const std::string& secret_base32, char* out_code, size_t out_code_size);

/// Seconds remaining in the CURRENT 30-second step (0-29) -- for a UI
/// countdown/auto-refresh indicator next to a displayed code. Based
/// purely on rtc_time::unix_time() % 30, so meaningful even if
/// generate() hasn't been called yet this step (still requires
/// rtc_time::is_synced() to mean anything, same as generate()).
uint32_t seconds_remaining();

} // namespace totp
