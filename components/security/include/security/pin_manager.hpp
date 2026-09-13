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
 * Staged anti-bruteforce policy, persisted across reboots so power-
 * cycling can't reset progress below the last reached checkpoint:
 *   - Failure 3 -> checkpoint saved to flash (NVS).
 *   - Failure 6 -> checkpoint saved to flash + 30-second lockout.
 *   - Failure 9 -> checkpoint saved to flash.
 *   - Failure 12 -> WipeRequired (no checkpoint write -- straight to
 *     the caller executing the wipe).
 *   - A successful PIN resets the RAM counter to 0, and clears the
 *     flash checkpoint IF one was set (a normal successful unlock
 *     that never hit a checkpoint writes nothing to flash at all).
 *
 * On boot, the flash checkpoint (not just the RAM counter) is what's
 * restored -- rebooting can only ever cost an attacker more time
 * (re-imposing the 30s lockout if checkpoint 6 was the last one saved),
 * never less. See pin_manager.cpp for the exact checkpoint values.
 */
VerifyResult verify(const char* pin);

/// Attempts remaining before the first 30-second lockout (now at 6
/// consecutive failures, not 7 -- see verify()'s doc comment). 0
/// while locked out or when the first threshold has already been
/// reached.
uint8_t attempts_remaining();

/// Attempts remaining before the automatic wipe (12 total consecutive
/// failures, not 17 -- see verify()'s doc comment). Meaningful mainly
/// once attempts_remaining() has reached 0 -- before that point the
/// two overlap. Callers (e.g. LockScreen) should show this once
/// attempts_remaining() hits 0, since a wrong guess in that range is
/// one step closer to an irreversible wipe with no other warning
/// otherwise.
uint8_t attempts_until_wipe();

bool is_locked_out();

} // namespace security::pin
