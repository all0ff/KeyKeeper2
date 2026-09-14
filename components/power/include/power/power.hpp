#pragma once

#include <cstdint>

// =============================================================================
// power
//
// Public power-management API: current power state, explicit sleep/
// shutdown (light/deep sleep, both manual-only now -- see below), and
// a callback registry so other components (display, vault, ...) can
// react to state changes -- e.g. re-lock the vault after waking.
//
// AUTOMATIC LIGHT SLEEP WAS REMOVED. It used to trigger on its own
// after an idle timeout via esp_light_sleep_start(), which turned out
// not to coexist reliably with the USB HID peripheral on this board:
// light sleep visibly disrupted the USB connection (the host would
// report "unknown USB device"), didn't reliably recover on wake, and
// the failure mode varied device to device -- confirmed as a real,
// reported problem, not a hypothetical. KeyKeeper 1.90's own approach
// was to cleanly detach USB before any real sleep; here the simpler
// and more robust fix is to not put the MCU to sleep for routine
// screen-timeout at all. request_sleep()/request_shutdown() (actual
// esp_light_sleep_start()/esp_deep_sleep_start()) still exist for
// explicit, deliberate use -- they're just no longer triggered
// automatically by inactivity.
//
// SCREEN TIMEOUT is now a separate, automatic mechanism this
// component still owns (same idle-tracking task, doesn't need a
// second one): after settings::all().general.display_off_timeout_s
// (read live every poll, 0 = disabled) of no input activity, it calls
// display::set_backlight(false) -- backlight only, MCU/USB/LVGL/every
// other task keep running completely normally. The next input turns
// the backlight back on. This is NOT a power::State transition (state
// stays Active throughout) -- it's purely a display action, with none
// of light sleep's peripheral-suspension side effects.
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
     * Reserved -- no longer drives anything automatically. Screen-off
     * timing now comes directly from
     * settings::all().general.display_off_timeout_s (read live, 0 =
     * disabled), not from a value captured once at init() time -- see
     * this header's file comment for why automatic light sleep itself
     * was removed. Kept as a field (rather than deleted outright) only
     * so existing callers passing a Config don't fail to compile;
     * assign it if you like, nothing reads it.
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
