#pragma once

#include "ui/screen.hpp"
#include "ui/widgets/keyboard.hpp"

// =============================================================================
// ui::screens::SetupPinScreen
//
// First-boot PIN setup screen (REQUIREMENTS 12.3, GUI.md section 8).
//
// Shown automatically on startup when no PIN is configured yet
// (security::pin::has_pin() == false). The user must set a PIN
// before they can access the device -- there is no "skip" path.
//
// Flow:
//   1. "Enter new PIN"    -> PinEntry widget
//   2. "Confirm PIN"      -> PinEntry widget (same length)
//   3. Match  -> set_pin() + unlock() -> MainMenu
//   4. Mismatch -> error message -> back to step 1
//
// BACK at step 1 with empty entry is a no-op (can't cancel setup).
// =============================================================================

namespace ui::screens {

class SetupPinScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Stage : uint8_t
    {
        EnterNew,
        Confirm,
        MismatchError,
    };

    void set_stage(Stage stage);
    void try_finish();
    void show_message(const char* msg);

    widgets::PinEntry pin_entry_;
    lv_obj_t* prompt_label_ = nullptr;
    lv_obj_t* message_label_ = nullptr;

    Stage stage_ = Stage::EnterNew;
    char first_pin_[widgets::PinEntry::MAX_LENGTH + 1]{};
};

} // namespace ui::screens
