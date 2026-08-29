#pragma once

#include <cstdint>

// =============================================================================
// power
//
// Public power-management API: current power state, idle-driven light
// sleep, explicit shutdown (deep sleep), and a callback registry so
// other components (display, vault, ...) can react to state changes --
// e.g. turn the backlight off before sleeping, or re-lock the vault
// after waking.
//
// The actual orchestration (idle timer task, esp_sleep configuration,
// wake sources) lives in power_manager.hpp/.cpp (private to this
// component). power.hpp/.cpp is a thin public-facing wrapper around
// that internal singleton.
//
// Activity tracking: this component polls input::last_activity_ms()
// (a non-consuming timestamp) in its own background task, so
// encoder/button activity resets the idle timer automatically without
// competing with the real event consumer for input's event queue.
// Components that generate other kinds of "the user is doing
// something" activity (e.g. USB HID typing) should call
// power::notify_activity() explicitly.
// =============================================================================

namespace power {

enum class State : uint8_t
{
    Active,     ///< Normal operation.
    LightSleep, ///< CPU/peripherals suspended, RAM retained, wakes on input.
    DeepSleep,  ///< Full power-down except RTC. Waking resets the chip.
};

/**
 * @brief Callback invoked on every power state transition.
 *
 * Called from the power task's context, not an ISR -- safe to call
 * most APIs, but keep it fast: it runs before the corresponding sleep
 * mode is actually entered (for LightSleep/DeepSleep transitions) or
 * right after waking (for the transition back to Active).
 */
using Callback = void (*)(State new_state, void* ctx);

struct Config
{
    /**
     * Milliseconds of no input activity before automatically entering
     * LightSleep. Set to 0 to disable automatic light sleep entirely
     * (only request_sleep() / request_shutdown() will change state).
     */
    uint32_t idle_timeout_ms = 30'000;
};

/**
 * @brief Initialize the power manager and start its background task.
 *
 * Must be called after input::init(). Safe to call once; a second
 * call is a no-op that returns true.
 */
bool init(const Config& cfg = Config{});

bool is_initialized();

/**
 * @brief Return the current power state.
 *
 * Note: while a call is in LightSleep, the calling task is itself
 * suspended (that's what light sleep means), so in practice this only
 * observably returns LightSleep to a callback registered via
 * register_callback().
 */
State state();

/**
 * @brief Reset the idle timer, as if the user had just interacted with
 *        the device.
 *
 * Input events already do this automatically. Call this explicitly
 * for other activity the power component has no way to observe on its
 * own (e.g. actively sending USB HID keystrokes).
 */
void notify_activity();

/**
 * @brief Register a callback for power state transitions.
 *
 * @return A handle to pass to unregister_callback(), or -1 if the
 *         callback registry is full.
 */
int register_callback(Callback cb, void* ctx);

void unregister_callback(int handle);

/**
 * @brief Enter LightSleep immediately, regardless of the idle timer.
 *
 * Returns once the device has woken back up. Typically called from a
 * "lock now" menu action.
 */
void request_sleep();

/**
 * @brief Enter DeepSleep immediately.
 *
 * Fires all registered callbacks with State::DeepSleep, then powers
 * down. This function does not return -- waking from deep sleep
 * restarts the firmware from app_main(), it does not resume execution
 * here.
 */
[[noreturn]] void request_shutdown();

} // namespace power
