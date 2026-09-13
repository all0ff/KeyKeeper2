#pragma once

#include "ui/async_pin_check.hpp"
#include "ui/screen.hpp"
#include "ui/widgets/keyboard.hpp"

// =============================================================================
// ui::screens::LockScreen
//
// docs/GUI.md section 8. Pushed from QuickScreen's Unlock action.
// Uses widgets::PinEntry for digit-by-digit PIN entry (see that
// widget's header for why: no touchscreen, no physical numpad, just
// one encoder).
//
// PIN verification runs via ui::AsyncPinCheck, not a direct blocking
// security::lock::unlock() call -- see that class's header comment
// for why: PBKDF2 takes ~10 seconds, and calling it directly from
// on_input() used to freeze the ENTIRE screen for that whole time
// (ui.cpp's ui_task holds display::lvgl_port's lock while dispatching
// input, which display::lvgl_port's own redraw task also needs).
// While a check is in flight, on_input() shows "Checking..." and
// ignores further input, including BackShort -- AsyncPinCheck is
// memory-safe if its owning Screen is destroyed mid-check regardless,
// but there's no reason to let that happen here.
//
// On a successful security::lock::unlock(), replaces itself with
// MainMenu (GUI.md 8/9) -- no back-stack entry is left for LockScreen
// itself, so BACK from Main Menu goes to QuickScreen, not back into
// the PIN screen.
//
// If the entered PIN is the configured DURESS PIN instead of the
// regular one (security::pin::VerifyResult::DuressTriggered -- see
// pin_manager.hpp), this screen wipes the vault and proceeds to
// MainMenu exactly as if it were a real success -- no visible
// difference. See handle_result()'s own comment.
// =============================================================================

namespace ui::screens {

class LockScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    void try_unlock();
    void handle_result(security::pin::VerifyResult result);
    static void on_check_done(security::pin::VerifyResult result, void* ctx);

    widgets::PinEntry pin_entry_;
    lv_obj_t* message_label_ = nullptr;

    AsyncPinCheck async_check_;
    bool checking_ = false;
};

} // namespace ui::screens
