#pragma once

#include "ui/screen.hpp"
#include "ui/widgets/keyboard.hpp"

#include <cstdint>
#include <string>

// =============================================================================
// ui::screens::SecuritySettingsScreen
//
// docs/GUI.md 14's Security Settings: Change PIN, Auto Lock Timeout,
// Security Options. "All changes go through SecurityService" (GUI.md
// 14) -- Change PIN calls security::pin::set_pin() directly and
// immediately (it's a security-sensitive action with its own
// success/failure feedback, not a field deferred to a "Save" step).
// Auto Lock enabled/timeout ARE deferred to Save, since they're
// ordinary settings::SecuritySettings fields (see
// GeneralSettingsScreen's header comment for why "Auto Lock" lives
// here rather than under General, despite GUI.md's section layout).
//
// "Security Options" (GUI.md 14) maps to
// settings::SecuritySettings::web_ui_permissions, a bitmask
// (settings_types.hpp) -- simplified here to a single on/off toggle
// (VIEW_ACCOUNTS vs NONE) rather than exposing all 4 permission bits
// individually. Not a considered policy, just what fits a one-row
// toggle; revisit once a real Web UI exists and there's an actual
// opinion on what each bit should gate.
//
// Change PIN flow: current PIN (skipped entirely if !has_pin(), i.e.
// first-time setup) -> new PIN -> confirm new PIN, each via
// widgets::PinEntry. BackShort with nothing typed at any step cancels
// the whole flow, discarding everything entered so far -- consistent
// with how BACK behaves elsewhere in this UI (AccountEditScreen,
// LockScreen).
// =============================================================================

namespace ui::screens {

class SecuritySettingsScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Row : uint8_t
    {
        ChangePin,
        AutoLockEnabled,
        AutoLockTimeout,
        WebUiViewAccounts,
        Save,
    };
    static constexpr size_t ROW_COUNT = 5;

    enum class Mode : uint8_t
    {
        Browse,
        Adjust,
        ChangingPin,
    };

    enum class ChangePinStep : uint8_t
    {
        Old,
        New,
        Confirm,
    };

    void build_rows();
    void render_rows();
    void move_selection(int32_t delta);
    void adjust_value(int32_t delta);
    void activate();
    void save();

    void begin_change_pin();
    void show_pin_step();
    void handle_pin_step_complete();
    void cancel_change_pin();

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    size_t selected_row_ = 0;
    Mode mode_ = Mode::Browse;

    // Working copy -- deferred fields only, persisted on Save.
    bool auto_lock_enabled_ = true;
    uint32_t auto_lock_timeout_s_ = 30;
    bool web_ui_view_accounts_ = false;

    ChangePinStep change_step_ = ChangePinStep::Old;
    widgets::PinEntry pin_entry_;
    std::string old_pin_;
    std::string new_pin_;
};

} // namespace ui::screens
