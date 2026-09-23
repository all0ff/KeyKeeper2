#pragma once

#include "ui/screen.hpp"

// =============================================================================
// ui::screens::QuickScreen
//
// docs/GUI.md section 7 ("Main Screen") -- the home/idle screen shown
// after boot and always at the bottom of the navigation stack.
// Reflects the current lock state; does not decide it itself (asks
// security::lock::state()).
//
// Quick Actions per GUI.md 7: Short BACK -> Unlock, Long BACK -> Print
// Password, Short OK -> Print URL.
//
// RESOLVED SPEC CONFLICT: GUI.md's own mockup shows these Print
// actions as hints on the LOCKED state of this screen (implying a
// "quick print without fully unlocking" convenience), while
// REQUIREMENTS.md 9.3 gates Print Password behind SecurityService.
// Neither action here is permission-gated, on purpose, though for two
// different reasons: Print URL (OkShort) only types the Web UI's
// network address, not stored vault data -- KeyKeeper 1.90's own
// reference implementation confirms this was meant to work whether or
// not a PIN had been entered yet. Print Password (BackLong) DOES type
// a real secret, but it's KeyKeeper 1.90's own separately-named
// "quick password without PIN" (quickpass) feature -- a single,
// explicitly-configured settings::UsbSettings::default_password field
// (see its own doc comment), not a specific vault entry's password;
// the person configuring it is deliberately accepting that trade-off,
// same reasoning 1.90 already established.
//
// PLACEHOLDER (not yet built):
//   - Print Password's "Quick Mode account" is still a fixed
//     settings::all().usb.default_password, not a selectable specific
//     vault entry.
// =============================================================================

namespace ui::screens {

class QuickScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    void refresh();

    lv_obj_t* state_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;

    // No footer_buf_ any more -- see footer_hint()'s own comment in
    // quick_screen.cpp for why copying into a fixed buffer was itself
    // the bug (a translation longer than the buffer got silently
    // truncated), not just something that needed a bigger size.
};

} // namespace ui::screens
