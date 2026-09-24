#pragma once

#include "security/pin_manager.hpp"

#include <cstdint>

// =============================================================================
// security::lock -- LockManager
//
// Per docs/ARCHITECTURE.md: Lock/Unlock, automatic lock, lock-at-
// startup, and the Locked/Unlocked state itself.
//
// Auto-lock is driven by its own idle timer here, separate from
// components/power's light-sleep idle timer -- they're related but
// distinct concerns (security lock vs. power saving), each reading
// input::last_activity_ms() independently (that call is non-
// destructive and safe for multiple readers, see input.hpp). Auto-
// lock behavior (enabled/timeout) is read from
// settings::all().security and refreshed live whenever
// event_bus::SystemEventId::SettingsChanged(Section::Security) fires.
//
// On successful unlock, begins a session via session_manager. On
// lock (manual or automatic), ends it. Publishes
// event_bus::SystemEventId::DeviceLocked/DeviceUnlocked.
// =============================================================================

namespace security::lock {

enum class State : uint8_t
{
    Locked,
    Unlocked,
};

using Callback = void (*)(State new_state, void* ctx);

/**
 * @brief Start the auto-lock task. Device starts Locked, per
 *        REQUIREMENTS 9.2 ("блокировка при запуске").
 *
 * Must be called after pin::init(), settings::init(), input::init(),
 * and event_bus::init().
 */
bool init();

bool is_initialized();

State state();

/**
 * @brief Attempt to unlock with a PIN guess. Delegates to
 *        security::pin::verify() -- and, first, security::pin::verify_duress()
 *        if a duress PIN is configured (see pin_manager.hpp's
 *        VerifyResult::DuressTriggered).
 *
 * On pin::VerifyResult::Success OR DuressTriggered: transitions to
 * Unlocked, begins a session, publishes DeviceUnlocked -- identically
 * in both cases, deliberately (see DuressTriggered's doc comment for
 * why). The caller is responsible for wiping the vault when
 * DuressTriggered is returned; this function does not (security::
 * must not depend on vault::).
 */
pin::VerifyResult unlock(const char* pin);

/**
 * @brief Transition directly to Unlocked WITHOUT calling
 *        pin::verify() -- for the one legitimate case where a fresh
 *        PIN was JUST established via pin::set_pin() (which already
 *        proved knowledge of the old PIN, if one existed) in the same
 *        logical flow. Re-verifying that brand-new PIN with another
 *        full PBKDF2 pass immediately afterward is provably redundant
 *        (~10 seconds wasted for nothing -- see pin_manager.hpp).
 *
 * NOT a general unlock bypass -- this must only ever be called
 * immediately after set_pin() returns true for a new PIN, never as a
 * substitute for unlock() anywhere else. No-op (returns false) if
 * pin::has_pin() is false, as a minimal misuse guard.
 */
bool unlock_after_pin_set();

/**
 * @brief Lock immediately (manual "lock now", or called internally by
 *        the auto-lock timer).
 *
 * No-op if already Locked. Ends the current session, publishes
 * DeviceLocked.
 */
void lock();

/**
 * @brief Record activity from a source OTHER than the physical
 *        rotary knob/buttons -- specifically, an authenticated Web UI
 *        request. auto_lock_task()'s own idle calculation used to
 *        read ONLY input::last_activity_ms(), meaning using the
 *        device purely through the Web UI (no physical button
 *        touched at all) still auto-locked on schedule regardless of
 *        how actively the web session was being used -- confirmed on
 *        real hardware as a real bug, not just an inconvenience: the
 *        very next poll after a web-based unlock would immediately
 *        re-lock again (input's own timestamp was still old, nothing
 *        about the successful web unlock had touched it), which is
 *        why unlocking via the web looked like it briefly worked and
 *        then instantly locked again on the next action. Called from
 *        each require_unlocked() in the web:: routes, right after
 *        confirming the request is already authenticated -- not on
 *        the login attempt itself, only on requests that prove an
 *        existing session is actively being used.
 */
void notify_activity();

int register_callback(Callback cb, void* ctx);
void unregister_callback(int handle);

} // namespace security::lock
