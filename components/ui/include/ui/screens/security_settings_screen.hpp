#pragma once

#include "ui/async_pin_check.hpp"
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
//
// The Old-PIN and set_pin() steps both run via ui::AsyncPinCheck, not
// direct blocking security::pin::verify()/set_pin() calls -- same
// ~10-second PBKDF2 freeze reasoning as LockScreen (see that class's
// header comment and AsyncPinCheck's own). While either check is in
// flight, on_input() ignores everything, same as LockScreen.
//
// Factory Reset: wipes vault.db, the PIN, and all settings (including
// WiFi credentials) back to defaults, then restarts -- same
// press-OK-twice confirm pattern as AccountViewScreen's Delete and
// BackupScreen's Restore (no confirmation dialog widget exists yet).
// Not part of docs/GUI.md 14 at all -- added at the project owner's
// request.
//
// Duress PIN: configures security::pin's alternate PIN (see
// pin_manager.hpp's VerifyResult::DuressTriggered) that silently
// wipes the vault on the Unlock screen instead of granting access.
// Flow: current PIN -> new duress PIN -> confirm, each via
// widgets::PinEntry. The CURRENT PIN is verified immediately after
// the first step via AsyncPinCheck, so an incorrect current PIN cannot
// advance to Duress PIN configuration. The final step still calls
// security::pin::set_duress_pin(), which verifies the current PIN again
// before storing the new fast-hash Duress PIN. This keeps the existing
// security::pin API unchanged and makes the user-facing current-PIN
// step behave correctly. Deliberately kept as separate state
// (Mode::SettingDuressPin, DuressPinStep, duress_* members) rather
// than reusing ChangePin's, to avoid any chance of the two flows'
// state bleeding into each other. Only ever REPLACES the duress PIN,
// no separate "remove" action in this UI yet --
// security::pin::clear_duress_pin() exists and works, just isn't
// wired to a button here.
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
        DuressPin,
        FactoryReset,
        AutoLock,
        WebUiViewAccounts,
        Save,
    };
    static constexpr size_t ROW_COUNT = 6;

    enum class Mode : uint8_t
    {
        Browse,
        Adjust,
        ChangingPin,
        SettingDuressPin,
    };

    enum class ChangePinStep : uint8_t
    {
        Old,
        New,
        Confirm,
    };

    enum class DuressPinStep : uint8_t
    {
        CurrentPin,
        EnterNew,
        Confirm,
    };

    void build_rows();
    void render_rows();
    void move_selection(int32_t delta);
    void adjust_value(int32_t delta);
    void activate();
    void save();

    void begin_change_pin();
    void show_pin_step(const char* error = nullptr);
    void handle_pin_step_complete();
    void handle_old_pin_result(security::pin::VerifyResult result);
    static void on_old_pin_check_done(security::pin::VerifyResult result, void* ctx);
    void handle_set_pin_result(security::pin::VerifyResult result);
    static void on_set_pin_done(security::pin::VerifyResult result, void* ctx);
    void cancel_change_pin();
    void perform_factory_reset();

    void begin_duress_pin_setup();
    void show_duress_pin_step(const char* error = nullptr);
    void handle_duress_pin_step_complete();
    void handle_duress_current_pin_result(security::pin::VerifyResult result);
    static void on_duress_current_pin_check_done(security::pin::VerifyResult result, void* ctx);
    void handle_duress_set_result(security::pin::VerifyResult result);
    static void on_duress_set_done(security::pin::VerifyResult result, void* ctx);
    void cancel_duress_pin_setup();

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    size_t selected_row_ = 0;
    Mode mode_ = Mode::Browse;

    // Working copy -- deferred fields only, persisted on Save.
    ///bool auto_lock_enabled_ = true;
    uint32_t auto_lock_timeout_s_ = 30;
    bool web_ui_view_accounts_ = false;

    ChangePinStep change_step_ = ChangePinStep::Old;
    widgets::PinEntry pin_entry_;
    std::string old_pin_;
    std::string new_pin_;

    DuressPinStep duress_step_ = DuressPinStep::CurrentPin;
    std::string duress_current_pin_;
    std::string duress_new_pin_;

    AsyncPinCheck async_check_;
    bool checking_ = false;
    uint8_t previous_pin_length_ = 0; // for rollback if set_pin() fails -- see the Confirm step

    bool factory_reset_confirm_pending_ = false;
};

} // namespace ui::screens
