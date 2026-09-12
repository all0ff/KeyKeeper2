#pragma once

#include <cstdint>

// =============================================================================
// security::pin -- PinManager
//
// Stores and verifies the device PIN and enforces the staged
// anti-bruteforce policy. PIN verification is separate from the future
// vault-encryption key hierarchy.
// =============================================================================

namespace security::pin {

enum class VerifyResult : uint8_t
{
    Success,
    WrongPin,
    LockedOut,
    WipeRequired,
    NoPinSet,
};

bool init();
bool is_initialized();
bool has_pin();

/**
 * @brief Set or replace the PIN.
 *
 * new_pin must contain only digits and have the configured PIN length.
 * If a PIN already exists, old_pin must verify successfully.
 */
bool set_pin(const char* new_pin, const char* old_pin);

/**
 * @brief Remove the stored PIN and reset PIN failure state.
 *
 * Intended for the completed automatic-wipe path. This does not modify
 * settings or vault storage.
 */
bool wipe();

/**
 * @brief Verify a PIN guess.
 *
 * Policy:
 *   - 7 consecutive failures start a 30-second lockout.
 *   - The failure counter remains at 7 after lockout expiry.
 *   - Failures 8..16 continue counting in RAM.
 *   - Failure 17 returns WipeRequired.
 *   - A successful PIN resets the failure counter.
 */
VerifyResult verify(const char* pin);

/// Attempts remaining before the first 30-second lockout. 0 while
/// locked out or when the first threshold has already been reached.
uint8_t attempts_remaining();

/// Attempts remaining before the automatic wipe (17 total consecutive
/// failures). Meaningful mainly once attempts_remaining() has reached
/// 0 -- before that point the two overlap. Callers (e.g. LockScreen)
/// should show this once attempts_remaining() hits 0, since a wrong
/// guess in that range is one step closer to an irreversible wipe
/// with no other warning otherwise.
uint8_t attempts_until_wipe();

bool is_locked_out();

} // namespace security::pin
