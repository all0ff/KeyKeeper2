#pragma once

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
// On a successful security::lock::unlock(), replaces itself with
// MainMenu (GUI.md 8/9) -- no back-stack entry is left for LockScreen
// itself, so BACK from Main Menu goes to QuickScreen, not back into
// the PIN screen.
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

    widgets::PinEntry pin_entry_;
    lv_obj_t* message_label_ = nullptr;
};

} // namespace ui::screens
