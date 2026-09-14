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

    /// The DURESS PIN was entered, not the regular one. Only ever
    /// returned by security::lock::unlock() -- see that function's
    /// doc comment for why security::pin::verify() itself never
    /// returns this (it's not what "verify a PIN" means in general,
    /// only in the specific context of an unlock attempt).
    ///
    /// The caller (ui::screens::LockScreen) MUST treat this exactly
    /// like Success -- same screen transition, same log tone, no
    /// visible "wiped" message -- except it ALSO wipes the vault
    /// (vault::repository::wipe() -- NOT the PIN itself, so the
    /// device keeps behaving completely normally afterward, same PIN
    /// still works). The whole point of a duress PIN is that entering
    /// it looks, from the outside, exactly like entering the real
    /// one -- see pin_manager.cpp's duress PIN section for the full
    /// design reasoning (why the vault is wiped, not the PIN; why the
    /// duress PIN doesn't reset on its own after triggering; why it
    /// resets when the regular PIN changes).
    DuressTriggered,
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

/**
 * @brief Duress PIN -- an alternate PIN, same length as the regular
 *        one, that silently wipes the vault when entered at unlock
 *        instead of granting real access to it. See VerifyResult's
 *        DuressTriggered value and pin_manager.cpp for the full
 *        design.
 */
bool has_duress_pin();

/**
 * @brief Configure (or replace) the duress PIN.
 *
 * current_pin must verify successfully against the REGULAR PIN first
 * (same "prove you already know it" gate set_pin() uses for its own
 * old_pin parameter). duress_pin must be the same length as
 * current_pin's actual, verified length (not settings::all().security.pin_length,
 * which could disagree with it) and must NOT equal current_pin --
 * an identical duress/regular PIN would silently wipe the vault on
 * every normal unlock, which defeats the entire point.
 */
bool set_duress_pin(const char* duress_pin, const char* current_pin);

/**
 * @brief Clear the stored duress PIN, if any.
 *
 * Called automatically by set_pin() whenever the regular PIN changes
 * (see that function's doc comment) -- also callable directly (e.g.
 * from Security Settings) to disable the feature without changing
 * the regular PIN.
 */
bool clear_duress_pin();

/// Used only by security::lock::unlock() -- see VerifyResult's
/// DuressTriggered doc comment for why this is a separate function
/// from verify() rather than folded into it (verify() is also used
/// for re-confirming the current PIN in non-unlock contexts, e.g.
/// Change PIN's old-PIN step, where a duress match must NOT be
/// treated specially).
bool verify_duress(const char* pin);

/**
 * @brief Burn roughly one PBKDF2 pass' worth of wall-clock time,
 *        without touching any stored state or the failure counter.
 *
 * Used only by security::lock::unlock() right after a duress-PIN
 * match, to keep VerifyResult::DuressTriggered's timing
 * indistinguishable from Success/WrongPin. verify_duress() is
 * deliberately fast (salted SHA-256, not PBKDF2 -- see
 * pin_manager.cpp) since the duress PIN itself doesn't need
 * brute-force resistance, but that speed difference would otherwise
 * make a triggered duress PIN complete almost instantly compared to
 * every other unlock attempt (~10s) -- exactly the kind of observable
 * difference the whole feature exists to avoid (someone coercing the
 * owner could notice "that was unusually fast"). Calling this after
 * a duress match closes that gap without slowing down the duress
 * CHECK itself or affecting security::pin's lockout/wipe counter.
 */
void consume_pbkdf2_time();

} // namespace security::pin
