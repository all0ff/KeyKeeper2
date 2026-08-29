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
 *        security::pin::verify().
 *
 * On pin::VerifyResult::Success: transitions to Unlocked, begins a
 * session, publishes DeviceUnlocked.
 */
pin::VerifyResult unlock(const char* pin);

/**
 * @brief Lock immediately (manual "lock now", or called internally by
 *        the auto-lock timer).
 *
 * No-op if already Locked. Ends the current session, publishes
 * DeviceLocked.
 */
void lock();

int register_callback(Callback cb, void* ctx);
void unregister_callback(int handle);

} // namespace security::lock
