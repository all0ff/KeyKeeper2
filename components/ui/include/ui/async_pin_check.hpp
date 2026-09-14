#pragma once

#include "security/pin_manager.hpp"

#include "lvgl.h"

#include <cstdint>

// =============================================================================
// ui::AsyncPinCheck
//
// Runs security::lock::unlock() or security::pin::verify() (both
// ~10 seconds of PBKDF2 work, see security::pin::pin_manager.cpp's
// PBKDF2_ITERATIONS) on a dedicated FreeRTOS task, so the caller's
// Screen::on_input() can return immediately instead of blocking.
//
// WHY THIS EXISTS: ui.cpp's ui_task calls Screen::on_input() while
// holding display::lvgl_port's lock (see ui.cpp's dispatch_event()).
// display::lvgl_port.cpp's OWN dispatcher task needs that SAME lock
// to call lv_timer_handler() -- the function that actually redraws
// anything. So a ~10-second blocking call inside on_input() doesn't
// just delay input processing, it freezes the entire screen (nothing
// redraws) for that whole time. Moving the blocking call to a
// separate task, and returning from on_input() right away, fixes
// this: the dispatcher task can keep redrawing (e.g. a "Checking..."
// message) while the worker task grinds through PBKDF2 in the
// background.
//
// Completion delivery: an LVGL timer (lv_timer_create()) polls a
// result flag every 100ms. LVGL timers run under the dispatcher
// task's own lock, so it's safe to touch LVGL objects (e.g. update a
// label) directly inside the completion callback -- no manual
// lvgl_port::lock()/unlock() needed there.
//
// LIFETIME: the PIN + result are held in a small heap-allocated block
// separate from the AsyncPinCheck object itself, specifically so that
// destroying the AsyncPinCheck (e.g. because its owning Screen was
// popped) while a check is still in flight doesn't leave the worker
// task writing into freed memory -- see async_pin_check.cpp. Even so,
// callers should avoid letting their owning Screen be destroyed while
// is_running() is true (e.g. LockScreen ignores BackShort while
// checking) -- it's memory-safe either way, but a result nobody will
// ever see is still wasted work.
//
// One AsyncPinCheck instance handles ONE in-flight check at a time;
// don't call start() again before the previous check's callback has
// fired (is_running() tells you).
// =============================================================================

namespace ui {

class AsyncPinCheck
{
public:
    enum class Kind : uint8_t
    {
        Unlock,              ///< security::lock::unlock(pin) -- transitions device state on success.
        Verify,              ///< security::pin::verify(pin) -- read-only check, doesn't change lock state.
        SetPin,              ///< security::pin::set_pin(new_pin, old_pin) -- see start_set_pin().
        SetPinAfterVerify,   ///< security::pin::set_pin_after_verify(new_pin) -- see start_set_pin_after_verify().
        SetDuressPin,        ///< security::pin::set_duress_pin(duress_pin, current_pin) -- see start_set_duress_pin().
        SetDuressPinAfterVerify, ///< security::pin::set_duress_pin_after_verify(...) -- see start_set_duress_pin_after_verify().
    };

    using ResultCallback = void (*)(security::pin::VerifyResult result, void* ctx);

    ~AsyncPinCheck();

    /// pin is copied internally (max 8 digits, matching
    /// widgets::PinEntry::MAX_LENGTH) -- safe to let the caller's own
    /// PIN buffer go away immediately after this call returns.
    void start(Kind kind, const char* pin, ResultCallback on_done, void* ctx);

    /**
     * @brief Async wrapper for security::pin::set_pin(new_pin, old_pin)
     *        specifically -- the only operation here needing two PINs,
     *        so it gets its own entry point rather than overloading
     *        start()'s single-PIN signature.
     *
     * Pass old_pin as nullptr for first-time setup (no PIN exists
     * yet) -- same meaning as calling security::pin::set_pin()
     * directly with a null old_pin. The callback receives
     * VerifyResult::Success on success, WrongPin on any failure (set_pin()
     * itself only returns bool; this maps false -> WrongPin so callers
     * can keep using the same VerifyResult-based callback as the other
     * two kinds -- don't read anything more specific than
     * success/failure into that mapping).
     *
     * This does NOT also call security::lock::unlock_after_pin_set()
     * -- that's fast (no PBKDF2) and callers should call it directly
     * from their callback if appropriate (e.g. SetupPinScreen does;
     * SecuritySettingsScreen's Change PIN does not, since the device
     * is already Unlocked to reach that screen at all).
     */
    void start_set_pin(const char* new_pin, const char* old_pin, ResultCallback on_done, void* ctx);

    /**
     * @brief Async wrapper for security::pin::set_pin_after_verify(new_pin)
     *        -- skips old-PIN re-verification entirely (see that
     *        function's own doc comment: only safe when the caller
     *        already verified the old PIN itself, separately). Cuts a
     *        Change-PIN flow's final step from ~20s (verify + hash)
     *        down to ~10s (hash only).
     */
    void start_set_pin_after_verify(const char* new_pin, ResultCallback on_done, void* ctx);

    /**
     * @brief Async wrapper for security::pin::set_duress_pin(duress_pin,
     *        current_pin) -- shares SetPin's "two PINs, bool result
     *        mapped to Success/WrongPin" shape, just a different
     *        underlying pin_manager call. current_pin is required
     *        (unlike start_set_pin()'s old_pin, which may legitimately
     *        be nullptr for first-time setup) -- set_duress_pin()
     *        always needs to verify it.
     */
    void start_set_duress_pin(const char* duress_pin, const char* current_pin, ResultCallback on_done, void* ctx);

    /**
     * @brief Async wrapper for security::pin::set_duress_pin_after_verify(...)
     *        -- skips the current-PIN re-verification set_duress_pin()
     *        would otherwise do (see that function's own doc comment).
     *        current_pin's VALUE is still required (for the
     *        length-match/distinctness checks), just not re-verified.
     *        The duress hash itself is fast (SHA-256) -- this cuts a
     *        duress-setup flow's final step to near-instant instead of
     *        the ~10s a redundant PBKDF2 re-verify would otherwise add.
     */
    void start_set_duress_pin_after_verify(const char* duress_pin, const char* current_pin, ResultCallback on_done,
                                            void* ctx);

    bool is_running() const { return running_; }

private:
    struct SharedState;

    static void task_entry(void* arg);
    static void timer_callback(lv_timer_t* timer);
    void launch(SharedState* state, ResultCallback on_done, void* ctx);

    SharedState* state_ = nullptr;
    lv_timer_t* poll_timer_ = nullptr;
    bool running_ = false;
};

} // namespace ui
