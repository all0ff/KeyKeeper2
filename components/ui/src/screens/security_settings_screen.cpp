#include "ui/screens/security_settings_screen.hpp"

#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/pin_manager.hpp"
#include "settings/settings.hpp"

#include "esp_log.h"

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.security_settings";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

constexpr int32_t TIMEOUT_STEP_S = 5;
constexpr uint32_t TIMEOUT_MIN_S = 5;
constexpr uint32_t TIMEOUT_MAX_S = 300; // placeholder range, not spec'd anywhere

} // namespace

const char* SecuritySettingsScreen::title() const
{
    return "Security";
}

const char* SecuritySettingsScreen::footer_hint() const
{
    switch (mode_) {
        case Mode::Adjust:
            return "ROTATE  Change    OK/BACK  Confirm";
        case Mode::ChangingPin:
            return "ROTATE  Digit    OK  Next    BACK  Erase/Cancel";
        default:
            return "OK  Open    BACK  Cancel";
    }
}

void SecuritySettingsScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    const settings::SecuritySettings& s = settings::all().security;
    auto_lock_enabled_ = s.auto_lock_enabled;
    auto_lock_timeout_s_ = s.auto_lock_timeout_s;
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
            case Row::AutoLockEnabled:
                lv_label_set_text_fmt(row_labels_[i], "%sAuto Lock: %s", prefix,
                                       auto_lock_enabled_ ? "On" : "Off");
                break;
            case Row::AutoLockTimeout:
                lv_label_set_text_fmt(row_labels_[i], "%sAuto Lock Timeout: %lus", prefix,
                                       static_cast<unsigned long>(auto_lock_timeout_s_));
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

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render_rows();
}

void SecuritySettingsScreen::adjust_value(int32_t delta)
{
    switch (static_cast<Row>(selected_row_)) {
        case Row::AutoLockEnabled:
            auto_lock_enabled_ = !auto_lock_enabled_;
            break;

        case Row::AutoLockTimeout: {
            int32_t value = static_cast<int32_t>(auto_lock_timeout_s_) + delta * TIMEOUT_STEP_S;
            if (value < static_cast<int32_t>(TIMEOUT_MIN_S)) {
                value = static_cast<int32_t>(TIMEOUT_MIN_S);
            }
            if (value > static_cast<int32_t>(TIMEOUT_MAX_S)) {
                value = static_cast<int32_t>(TIMEOUT_MAX_S);
            }
            auto_lock_timeout_s_ = static_cast<uint32_t>(value);
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
    updated.auto_lock_enabled = auto_lock_enabled_;
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

void SecuritySettingsScreen::show_pin_step()
{
    lv_obj_clean(content_parent_);

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

    widgets::PinEntry::Config cfg{};
    cfg.length = settings::all().security.pin_length;
    pin_entry_.init(content_parent_, cfg);
}

void SecuritySettingsScreen::handle_pin_step_complete()
{
    switch (change_step_) {
        case ChangePinStep::Old:
            old_pin_ = pin_entry_.pin();
            pin_entry_.reset();
            change_step_ = ChangePinStep::New;
            show_pin_step();
            return;

        case ChangePinStep::New:
            new_pin_ = pin_entry_.pin();
            pin_entry_.reset();
            change_step_ = ChangePinStep::Confirm;
            show_pin_step();
            return;

        case ChangePinStep::Confirm: {
            const std::string confirm_pin = pin_entry_.pin();
            pin_entry_.reset();

            const bool mismatch = (confirm_pin != new_pin_);
            bool ok = false;

            if (!mismatch) {
                ok = security::pin::set_pin(new_pin_.c_str(),
                                             old_pin_.empty() ? nullptr : old_pin_.c_str());
            }

            old_pin_.clear();
            new_pin_.clear();

            mode_ = Mode::Browse;
            lv_obj_clean(content_parent_);
            build_rows();

            if (mismatch) {
                ESP_LOGI(TAG, "PIN change cancelled -- confirmation did not match");
                lv_label_set_text(status_label_, "PINs did not match");
            } else if (ok) {
                ESP_LOGI(TAG, "PIN changed");
                lv_label_set_text(status_label_, "PIN changed");
            } else {
                ESP_LOGI(TAG, "PIN change failed");
                lv_label_set_text(status_label_, "PIN change failed (wrong current PIN?)");
            }
            return;
        }
    }
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

bool SecuritySettingsScreen::on_input(InputAction action)
{
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
