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
// OPEN SPEC CONFLICT (not resolved here, see components/ui/README.md):
// GUI.md's own mockup shows these Print actions as hints on the
// LOCKED state of this screen (implying a "quick print without fully
// unlocking" convenience), but REQUIREMENTS.md 9.3 gates
// Print Password behind SecurityService, and PermissionManager (as
// already built and confirmed) requires Unlocked + an active session
// for every gated operation, no exception for this screen. As
// implemented here, both actions call permission::check() as-is and
// will therefore be denied while Locked -- the safer default (never
// leak stored data from a locked device), but possibly not what
// GUI.md's mockup intended. Revisit once LockScreen exists and this
// is actually end-to-end testable.
//
// PLACEHOLDERS (not yet built):
//   - Print URL/Print Password need a concrete USB HID
//     OutputChannel (interfaces::channels has only the abstract
//     contract) -- the permission check runs for real, the actual USB
//     typing is a logged no-op.
//   - There is no separate "Print URL" permission operation in
//     PermissionManager (REQUIREMENTS 9.3 only lists Print Password/
//     Print OTP among the print-related gated ops) -- Print URL is
//     mapped to the same Operation::PrintPassword check here as a
//     placeholder, not a considered decision.
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

    char footer_buf_[48]{};
};

} // namespace ui::screens
