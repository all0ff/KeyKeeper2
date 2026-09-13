#include "ui/screens/security_settings_screen.hpp"

#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/pin_manager.hpp"
#include "settings/settings.hpp"
#include "vault/vault_repository.hpp"
#include "wifi/wifi_service.hpp"

#include "esp_log.h"
#include "esp_system.h"

#include <cstdio>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.security_settings";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

constexpr int32_t TIMEOUT_STEP_S = 5;
constexpr uint32_t TIMEOUT_MIN_S = 0;
constexpr uint32_t TIMEOUT_MAX_S = 300; // placeholder range, not spec'd anywhere

} // namespace

const char* SecuritySettingsScreen::title() const
{
    return "Security";
}

const char* SecuritySettingsScreen::footer_hint() const
{
    if (checking_) {
        return "Checking...";
    }
    switch (mode_) {
        case Mode::Adjust:
            return "ROTATE  Change    OK/BACK  Confirm";
        case Mode::ChangingPin:
            return "ROTATE Digit OK Next Hold OK Done BACK Erase/Cancel";
        case Mode::SettingDuressPin:
            return "ROTATE Digit OK Next Hold OK Done BACK Erase/Cancel";
        default:
            return "OK  Open    BACK  Cancel";
    }
}

void SecuritySettingsScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    const settings::SecuritySettings& s = settings::all().security;
    ///auto_lock_enabled_ = s.auto_lock_enabled;
    auto_lock_timeout_s_ = s.auto_lock_enabled ? s.auto_lock_timeout_s : 0;
    web_ui_view_accounts_ = (s.web_ui_permissions & settings::WEB_UI_VIEW_ACCOUNTS) != 0;

    build_rows();
}

void SecuritySettingsScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
}

void SecuritySettingsScreen::build_rows()
{
    for (size_t i = 0; i < ROW_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        row_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_rows();
}

void SecuritySettingsScreen::render_rows()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool is_selected = (i == selected_row_);
        const bool is_adjusting = is_selected && mode_ == Mode::Adjust;
        lv_obj_set_style_text_color(
            row_labels_[i], is_adjusting ? pal.warning : (is_selected ? pal.accent : pal.primary_text), 0);

        const char* prefix = is_selected ? "> " : "";

        switch (static_cast<Row>(i)) {
            case Row::ChangePin:
                lv_label_set_text_fmt(row_labels_[i], "%sChange PIN", prefix);
                break;

            case Row::DuressPin:
                lv_label_set_text_fmt(row_labels_[i], "%sDuress PIN: %s", prefix,
                                       security::pin::has_duress_pin() ? "Configured" : "Not set");
                break;

            case Row::FactoryReset:
                if (is_selected && factory_reset_confirm_pending_) {
                    lv_obj_set_style_text_color(row_labels_[i], pal.error, 0);
                    lv_label_set_text_fmt(row_labels_[i], "%sFactory Reset (confirm?)", prefix);
                } else {
                    lv_label_set_text_fmt(row_labels_[i], "%sFactory Reset", prefix);
                }
                break;

            case Row::AutoLock:
                if (auto_lock_timeout_s_ == 0) {
                    lv_label_set_text_fmt(row_labels_[i], "%sAuto Lock: off", prefix);
                } else {
                    lv_label_set_text_fmt(
                        row_labels_[i],
                        "%sAuto Lock: %lus",
                        prefix,
                        static_cast<unsigned long>(auto_lock_timeout_s_));
                }
                break;

            case Row::WebUiViewAccounts:
                lv_label_set_text_fmt(row_labels_[i], "%sWeb UI View: %s", prefix,
                                    web_ui_view_accounts_ ? "Allowed" : "Off");
                break;

            case Row::Save:
                lv_label_set_text_fmt(row_labels_[i], "%sSave", prefix);
                
                break;
        }
    }
}

void SecuritySettingsScreen::move_selection(int32_t delta)
{
    int32_t index = static_cast<int32_t>(selected_row_) + delta;
    const int32_t count = static_cast<int32_t>(ROW_COUNT);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_row_ = static_cast<size_t>(index);

    factory_reset_confirm_pending_ = false;
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render_rows();
}

void SecuritySettingsScreen::adjust_value(int32_t delta)
{
    switch (static_cast<Row>(selected_row_)) {
        
        case Row::AutoLock: {
            int32_t v = static_cast<int32_t>(auto_lock_timeout_s_);
            // до 30 с шаг 5, дальше шаг 15
            const int32_t step = (v >= 30) ? 15 * delta : 5 * delta;    // или + delta * TIMEOUT_STEP_S
            v += step;

            if (v < static_cast<int32_t>(TIMEOUT_MIN_S)) {
                v = static_cast<int32_t>(TIMEOUT_MIN_S);
            }

            if (v > static_cast<int32_t>(TIMEOUT_MAX_S)) {
                v = static_cast<int32_t>(TIMEOUT_MAX_S);
            }

            auto_lock_timeout_s_ = static_cast<uint32_t>(v);
            ///auto_lock_enabled_   = auto_lock_timeout_s_ > 0;
            break;
        }

        case Row::WebUiViewAccounts:
            web_ui_view_accounts_ = !web_ui_view_accounts_;
            break;

        default:
            break;
    }

    render_rows();
}

void SecuritySettingsScreen::activate()
{
    const auto row = static_cast<Row>(selected_row_);

    if (row == Row::ChangePin) {
        begin_change_pin();
        return;
    }
    if (row == Row::DuressPin) {
        begin_duress_pin_setup();
        return;
    }
    if (row == Row::FactoryReset) {
        if (!factory_reset_confirm_pending_) {
            factory_reset_confirm_pending_ = true;
            render_rows();
            lv_label_set_text(status_label_, "This erases EVERYTHING. Press OK again to confirm.");
            return;
        }
        perform_factory_reset();
        return;
    }
    if (row == Row::Save) {
        save();
        return;
    }

    mode_ = Mode::Adjust;
    render_rows();
}

void SecuritySettingsScreen::save()
{
    settings::SecuritySettings updated = settings::all().security;
    updated.auto_lock_enabled   = auto_lock_timeout_s_ > 0;
    updated.auto_lock_timeout_s = auto_lock_timeout_s_;
    updated.web_ui_permissions =
        web_ui_view_accounts_ ? settings::WEB_UI_VIEW_ACCOUNTS : settings::WEB_UI_NONE;

    if (settings::set_security(updated)) {
        ESP_LOGI(TAG, "Security settings saved");
        manager().pop();
    } else {
        lv_label_set_text(status_label_, "Save failed");
    }
}

void SecuritySettingsScreen::begin_change_pin()
{
    mode_ = Mode::ChangingPin;
    change_step_ = security::pin::has_pin() ? ChangePinStep::Old : ChangePinStep::New;
    old_pin_.clear();
    new_pin_.clear();
    show_pin_step();
}

void SecuritySettingsScreen::show_pin_step(const char* error /* = nullptr */)
{
    lv_obj_clean(content_parent_);
    status_label_ = nullptr;        // ← объект уничтожен, честно обнуляем

    const theme::Palette& pal = theme::current();
    lv_obj_t* header = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(header, pal.secondary_text, 0);

    const char* text = "";
    switch (change_step_) {
        case ChangePinStep::Old:     text = "Enter current PIN"; break;
        case ChangePinStep::New:     text = "Enter new PIN"; break;
        case ChangePinStep::Confirm: text = "Confirm new PIN"; break;
    }
    lv_label_set_text(header, text);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 4);

    if (error != nullptr) {                // ← ошибка показывается внутри шага
        lv_obj_t* err = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(err, pal.warning, 0);
        lv_label_set_text(err, error);
        lv_obj_align(err, LV_ALIGN_TOP_MID, 0, 24);
    }

    widgets::PinEntry::Config cfg{};

    if (change_step_ == ChangePinStep::Old) {
        cfg.length = settings::all().security.pin_length;

        if (cfg.length < 4 || cfg.length > 6) {
            cfg.length = 6;
        }

        cfg.min_length = cfg.length;
        cfg.finish_on_short = true;
    } else {
        cfg.length = 6;
        cfg.min_length = 4;
        cfg.finish_on_short = false;
    }


    pin_entry_.init(content_parent_, cfg);
}

void SecuritySettingsScreen::handle_pin_step_complete()
{
    switch (change_step_) {
        case ChangePinStep::Old: {
            old_pin_ = pin_entry_.pin();
            pin_entry_.reset();

            checking_ = true;
            // status_label_ is nullptr while in the PIN-step UI (see
            // show_pin_step()) -- reuse its own message-display path
            // instead of touching a null label directly.
            show_pin_step("Checking...");
            async_check_.start(AsyncPinCheck::Kind::Verify, old_pin_.c_str(),
                                &SecuritySettingsScreen::on_old_pin_check_done, this);
            return;
        }

        case ChangePinStep::New:
            new_pin_ = pin_entry_.pin();
            pin_entry_.reset();
            change_step_ = ChangePinStep::Confirm;
            show_pin_step();
            return;

        case ChangePinStep::Confirm: {
            const std::string confirm_pin = pin_entry_.pin();
            pin_entry_.reset();

            if (confirm_pin != new_pin_) {
                old_pin_.clear();
                new_pin_.clear();
                mode_ = Mode::Browse;
                lv_obj_clean(content_parent_);
                build_rows();
                ESP_LOGI(TAG, "PIN change cancelled -- confirmation did not match");
                lv_label_set_text(status_label_, "PINs did not match");
                return;
            }

            // Update the pin_length setting BEFORE calling set_pin():
            // set_pin() validates the new PIN's length against
            // settings::all().security.pin_length internally, so it
            // must already reflect the new length or a legitimately
            // shorter/longer new PIN would be rejected. Rolled back in
            // handle_set_pin_result() if set_pin() fails, so a failed
            // change never leaves pin_length out of sync with the
            // still-active (old) PIN.
            previous_pin_length_ = settings::all().security.pin_length;

            settings::SecuritySettings updated = settings::all().security;
            updated.pin_length = static_cast<uint8_t>(new_pin_.length());

            if (!settings::set_security(updated)) {
                ESP_LOGE(TAG, "Failed to save PIN length setting before set_pin()");
                old_pin_.clear();
                new_pin_.clear();
                mode_ = Mode::Browse;
                lv_obj_clean(content_parent_);
                build_rows();
                lv_label_set_text(status_label_, "PIN change failed");
                return;
            }

            checking_ = true;
            show_pin_step("Checking...");
            async_check_.start_set_pin(new_pin_.c_str(), old_pin_.empty() ? nullptr : old_pin_.c_str(),
                                        &SecuritySettingsScreen::on_set_pin_done, this);
            return;
        }
    }
}

void SecuritySettingsScreen::on_set_pin_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<SecuritySettingsScreen*>(ctx)->handle_set_pin_result(result);
}

void SecuritySettingsScreen::handle_set_pin_result(security::pin::VerifyResult result)
{
    checking_ = false;

    const bool ok = (result == security::pin::VerifyResult::Success);
    if (!ok) {
        settings::SecuritySettings rollback = settings::all().security;
        rollback.pin_length = previous_pin_length_;
        settings::set_security(rollback);
    }

    old_pin_.clear();
    new_pin_.clear();
    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows();

    if (ok) {
        ESP_LOGI(TAG, "PIN changed");
        lv_label_set_text(status_label_, "PIN changed");
    } else {
        ESP_LOGI(TAG, "PIN change failed");
        lv_label_set_text(status_label_, "PIN change failed");
    }
}

void SecuritySettingsScreen::on_old_pin_check_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<SecuritySettingsScreen*>(ctx)->handle_old_pin_result(result);
}

void SecuritySettingsScreen::handle_old_pin_result(security::pin::VerifyResult result)
{
    checking_ = false;

    if (result != security::pin::VerifyResult::Success) {
        old_pin_.clear();

        if (result == security::pin::VerifyResult::LockedOut) {
            show_pin_step("Locked out, try later");
        } else if (result == security::pin::VerifyResult::WipeRequired) {
            // NOTE: reaching the wipe threshold here (verifying the OLD
            // PIN while changing it) does NOT trigger an actual wipe --
            // only LockScreen's unlock path does. The device is already
            // Unlocked to reach this screen at all, so the vault isn't
            // protected by a wipe here either way; this is a deliberate
            // scope decision, not an oversight -- flagged for you to
            // confirm it's the behavior you want.
            show_pin_step("Too many failed attempts");
        } else {
            const uint8_t remaining = security::pin::attempts_remaining();
            char buf[48];
            if (remaining > 0) {
                std::snprintf(buf, sizeof(buf), "Wrong current PIN, %u left",
                              static_cast<unsigned>(remaining));
            } else {
                const uint8_t until_wipe = security::pin::attempts_until_wipe();
                std::snprintf(buf, sizeof(buf), "Wrong PIN! %u attempts until vault wipe",
                              static_cast<unsigned>(until_wipe));
            }
            show_pin_step(buf);
        }
        return;
    }

    change_step_ = ChangePinStep::New;
    show_pin_step();
}

void SecuritySettingsScreen::cancel_change_pin()
{
    old_pin_.clear();
    new_pin_.clear();
    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows();
    lv_label_set_text(status_label_, "PIN change cancelled");
}

void SecuritySettingsScreen::begin_duress_pin_setup()
{
    mode_ = Mode::SettingDuressPin;
    duress_step_ = DuressPinStep::CurrentPin;
    duress_current_pin_.clear();
    duress_new_pin_.clear();
    show_duress_pin_step();
}

void SecuritySettingsScreen::show_duress_pin_step(const char* error /* = nullptr */)
{
    lv_obj_clean(content_parent_);
    status_label_ = nullptr; // object just got destroyed above -- honestly null it out

    const theme::Palette& pal = theme::current();
    lv_obj_t* header = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(header, pal.secondary_text, 0);

    const char* text = "";
    switch (duress_step_) {
        case DuressPinStep::CurrentPin: text = "Enter current PIN"; break;
        case DuressPinStep::EnterNew:   text = "Enter duress PIN"; break;
        case DuressPinStep::Confirm:    text = "Confirm duress PIN"; break;
    }
    lv_label_set_text(header, text);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 4);

    if (error != nullptr) {
        lv_obj_t* err = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(err, pal.warning, 0);
        lv_label_set_text(err, error);
        lv_obj_align(err, LV_ALIGN_TOP_MID, 0, 24);
    }

    // Fixed at the CURRENT regular PIN's length for all three steps
    // -- "same length as the current PIN" is the whole point, unlike
    // Change PIN's New/Confirm steps, which allow a flexible 4-6
    // length since you're choosing a length there.
    widgets::PinEntry::Config cfg{};
    cfg.length = settings::all().security.pin_length;
    if (cfg.length < 4 || cfg.length > 6) {
        cfg.length = 6;
    }
    cfg.min_length = cfg.length;
    cfg.finish_on_short = true;

    pin_entry_.init(content_parent_, cfg);
}

void SecuritySettingsScreen::handle_duress_pin_step_complete()
{
    switch (duress_step_) {
        case DuressPinStep::CurrentPin:
            // Collected, not verified yet -- see security_settings_screen.hpp's
            // file comment for why (verification happens once, inside
            // the single async set_duress_pin() call at Confirm).
            duress_current_pin_ = pin_entry_.pin();
            pin_entry_.reset();
            duress_step_ = DuressPinStep::EnterNew;
            show_duress_pin_step();
            return;

        case DuressPinStep::EnterNew:
            duress_new_pin_ = pin_entry_.pin();
            pin_entry_.reset();
            duress_step_ = DuressPinStep::Confirm;
            show_duress_pin_step();
            return;

        case DuressPinStep::Confirm: {
            const std::string confirm_pin = pin_entry_.pin();
            pin_entry_.reset();

            if (confirm_pin != duress_new_pin_) {
                duress_current_pin_.clear();
                duress_new_pin_.clear();
                mode_ = Mode::Browse;
                lv_obj_clean(content_parent_);
                build_rows();
                lv_label_set_text(status_label_, "Duress PINs did not match");
                return;
            }

            checking_ = true;
            show_duress_pin_step("Checking...");
            async_check_.start_set_duress_pin(duress_new_pin_.c_str(), duress_current_pin_.c_str(),
                                               &SecuritySettingsScreen::on_duress_set_done, this);
            return;
        }
    }
}

void SecuritySettingsScreen::on_duress_set_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<SecuritySettingsScreen*>(ctx)->handle_duress_set_result(result);
}

void SecuritySettingsScreen::handle_duress_set_result(security::pin::VerifyResult result)
{
    checking_ = false;

    const bool ok = (result == security::pin::VerifyResult::Success);

    duress_current_pin_.clear();
    duress_new_pin_.clear();
    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows();

    if (ok) {
        ESP_LOGI(TAG, "Duress PIN configured");
        lv_label_set_text(status_label_, "Duress PIN set");
    } else {
        // Covers both "current PIN was wrong" and "duress PIN equals
        // the regular PIN" (set_duress_pin() rejects both, see
        // pin_manager.cpp) -- deliberately one generic message rather
        // than distinguishing them, so a wrong-current-PIN attempt
        // here doesn't leak useful timing/feedback beyond "it failed".
        ESP_LOGW(TAG, "Duress PIN setup failed");
        lv_label_set_text(status_label_, "Duress PIN setup failed");
    }
}

void SecuritySettingsScreen::cancel_duress_pin_setup()
{
    duress_current_pin_.clear();
    duress_new_pin_.clear();
    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows();
    lv_label_set_text(status_label_, "Duress PIN setup cancelled");
}

void SecuritySettingsScreen::perform_factory_reset()
{
    ESP_LOGW(TAG, "Factory reset requested from Security Settings");

    // Same order as the automatic brute-force wipe (LockScreen's
    // WipeRequired case): vault first, then the PIN, only proceeding
    // if the previous step succeeded.
    const bool vault_wiped = vault::repository::wipe();
    const bool pin_wiped = vault_wiped && security::pin::wipe();
    const bool settings_reset = pin_wiped && settings::reset_to_defaults();

    if (!settings_reset) {
        factory_reset_confirm_pending_ = false;
        ESP_LOGE(TAG, "Factory reset failed (vault=%d, pin=%d, settings=%d)", vault_wiped ? 1 : 0,
                  pin_wiped ? 1 : 0, settings_reset ? 1 : 0);
        lv_label_set_text(status_label_, "Factory reset failed");
        render_rows();
        return;
    }

    // settings::reset_to_defaults() only rewrites the STORED WiFi
    // config back to Disabled -- it doesn't stop an already-running
    // radio. apply_settings() re-reads that stored config and acts
    // on it immediately, matching WifiSettingsScreen's own Save.
    wifi::apply_settings();

    ESP_LOGW(TAG, "Factory reset complete -- restarting");
    lv_label_set_text(status_label_, "Reset complete. Restarting...");
    esp_restart(); // does not return -- same pattern as BackupScreen's Restore, no delay needed
}

bool SecuritySettingsScreen::on_input(InputAction action)
{
    if (checking_) {
        return true; // swallow everything while the async check runs
    }

    if (mode_ == Mode::ChangingPin) {
        const bool consumed = pin_entry_.on_input(action);

        if (pin_entry_.is_complete()) {
            handle_pin_step_complete();
            return true;
        }
        if (!consumed) {
            // BackShort with nothing typed at this step -- cancel the
            // whole change-PIN flow, not just this one step.
            cancel_change_pin();
            return true;
        }
        return true;
    }

    if (mode_ == Mode::SettingDuressPin) {
        const bool consumed = pin_entry_.on_input(action);

        if (pin_entry_.is_complete()) {
            handle_duress_pin_step_complete();
            return true;
        }
        if (!consumed) {
            cancel_duress_pin_setup();
            return true;
        }
        return true;
    }

    if (mode_ == Mode::Adjust) {
        switch (action) {
            case InputAction::RotateLeft:
                adjust_value(-1);
                return true;
            case InputAction::RotateRight:
                adjust_value(+1);
                return true;
            case InputAction::OkShort:
            case InputAction::BackShort:
                mode_ = Mode::Browse;
                render_rows();
                return true;
            default:
                return false;
        }
    }

    switch (action) {
        case InputAction::RotateLeft:
            move_selection(-1);
            return true;

        case InputAction::RotateRight:
            move_selection(+1);
            return true;

        case InputAction::OkShort:
            activate();
            return true;

        case InputAction::BackShort:
            return false; // pop, discarding unsaved auto-lock/permission changes (any PIN change already applied)

        default:
            return false;
    }
}

} // namespace ui::screens
