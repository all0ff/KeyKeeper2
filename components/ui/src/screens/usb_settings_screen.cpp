#include "ui/screens/usb_settings_screen.hpp"

#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "settings/settings.hpp"

#include "esp_log.h"

#include <cstring>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.usb_settings";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

constexpr int32_t DELAY_STEP_MS = 50;
constexpr int32_t DELAY_MIN_MS = 0;
constexpr int32_t DELAY_MAX_MS = 5000; // placeholder range, not spec'd anywhere

} // namespace

const char* UsbSettingsScreen::title() const
{
    return "USB";
}

const char* UsbSettingsScreen::footer_hint() const
{
    switch (mode_) {
        case Mode::Adjust:
            return "ROTATE  Change    OK/BACK  Confirm";
        case Mode::EditText:
            return "OK  Add char    Hold OK  Done    BACK  Erase";
        default:
            return "OK  Open    BACK  Cancel";
    }
}

void UsbSettingsScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    const settings::UsbSettings& u = settings::all().usb;
    std::strncpy(password_shortcut_, u.default_password, sizeof(password_shortcut_) - 1);
    password_shortcut_[sizeof(password_shortcut_) - 1] = '\0';
    delay_before_typing_ms_ = u.delay_before_typing_ms;
    delay_between_chars_ms_ = u.delay_between_chars_ms;
    delay_between_fields_ms_ = u.delay_between_fields_ms;
    typing_order_ = u.typing_order;

    build_rows(content_parent_);
}

void UsbSettingsScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
}

void UsbSettingsScreen::build_rows(lv_obj_t* parent)
{
    for (size_t i = 0; i < ROW_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(parent);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        row_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_rows();
}

const char* UsbSettingsScreen::typing_order_label() const
{
    switch (typing_order_) {
        case settings::TypingOrder::LoginTabPasswordEnter: return "Login+Tab+Password+Enter";
        case settings::TypingOrder::PasswordOnly:          return "Password Only";
        case settings::TypingOrder::PasswordEnter:         return "Password+Enter";
    }
    return "";
}

void UsbSettingsScreen::render_rows()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool is_selected = (i == selected_row_);
        const bool is_adjusting = is_selected && mode_ == Mode::Adjust;
        lv_obj_set_style_text_color(
            row_labels_[i], is_adjusting ? pal.warning : (is_selected ? pal.accent : pal.primary_text), 0);

        const char* prefix = is_selected ? "> " : "";

        switch (static_cast<Row>(i)) {
            case Row::PasswordShortcut:
                lv_label_set_text_fmt(row_labels_[i], "%sPassword Shortcut: %s", prefix,
                                       password_shortcut_[0] == '\0' ? "(empty)" : "********");
                break;
            case Row::DelayBeforeTyping:
                lv_label_set_text_fmt(row_labels_[i], "%sDelay Before Typing: %ums", prefix,
                                       static_cast<unsigned>(delay_before_typing_ms_));
                break;
            case Row::DelayBetweenChars:
                lv_label_set_text_fmt(row_labels_[i], "%sDelay Between Chars: %ums", prefix,
                                       static_cast<unsigned>(delay_between_chars_ms_));
                break;
            case Row::DelayBetweenFields:
                lv_label_set_text_fmt(row_labels_[i], "%sDelay Between Fields: %ums", prefix,
                                       static_cast<unsigned>(delay_between_fields_ms_));
                break;
            case Row::PrintSequence:
                lv_label_set_text_fmt(row_labels_[i], "%sPrint Sequence: %s", prefix, typing_order_label());
                break;
            case Row::Save:
                lv_label_set_text_fmt(row_labels_[i], "%sSave", prefix);
                break;
        }
    }
}

void UsbSettingsScreen::move_selection(int32_t delta)
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

void UsbSettingsScreen::adjust_value(int32_t delta)
{
    auto clamp_delay = [](int32_t v) {
        if (v < DELAY_MIN_MS) {
            return DELAY_MIN_MS;
        }
        if (v > DELAY_MAX_MS) {
            return DELAY_MAX_MS;
        }
        return v;
    };

    switch (static_cast<Row>(selected_row_)) {
        case Row::DelayBeforeTyping:
            delay_before_typing_ms_ = static_cast<uint16_t>(
                clamp_delay(static_cast<int32_t>(delay_before_typing_ms_) + delta * DELAY_STEP_MS));
            break;

        case Row::DelayBetweenChars:
            delay_between_chars_ms_ = static_cast<uint16_t>(
                clamp_delay(static_cast<int32_t>(delay_between_chars_ms_) + delta * DELAY_STEP_MS));
            break;

        case Row::DelayBetweenFields:
            delay_between_fields_ms_ = static_cast<uint16_t>(
                clamp_delay(static_cast<int32_t>(delay_between_fields_ms_) + delta * DELAY_STEP_MS));
            break;

        case Row::PrintSequence:
            switch (typing_order_) {
                case settings::TypingOrder::LoginTabPasswordEnter:
                    typing_order_ = settings::TypingOrder::PasswordOnly;
                    break;
                case settings::TypingOrder::PasswordOnly:
                    typing_order_ = settings::TypingOrder::PasswordEnter;
                    break;
                case settings::TypingOrder::PasswordEnter:
                    typing_order_ = settings::TypingOrder::LoginTabPasswordEnter;
                    break;
            }
            break;

        default:
            break;
    }

    render_rows();
}

void UsbSettingsScreen::activate()
{
    const auto row = static_cast<Row>(selected_row_);

    if (row == Row::PasswordShortcut) {
        enter_edit_password();
        return;
    }
    if (row == Row::Save) {
        save();
        return;
    }

    mode_ = Mode::Adjust;
    render_rows();
}

void UsbSettingsScreen::enter_edit_password()
{
    mode_ = Mode::EditText;
    lv_obj_clean(content_parent_);

    const theme::Palette& pal = theme::current();
    lv_obj_t* header = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(header, pal.secondary_text, 0);
    lv_label_set_text(header, "Editing: Password Shortcut");
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 4, 4);

    widgets::TextEntry::Config cfg{};
    cfg.max_length = sizeof(password_shortcut_) - 1;
    cfg.mask = true;
    cfg.initial_value = password_shortcut_;

    text_entry_.init(content_parent_, cfg);
    lv_obj_align(text_entry_.root(), LV_ALIGN_TOP_LEFT, 4, 24);
}

void UsbSettingsScreen::exit_edit_password(bool commit)
{
    if (commit) {
        std::strncpy(password_shortcut_, text_entry_.text(), sizeof(password_shortcut_) - 1);
        password_shortcut_[sizeof(password_shortcut_) - 1] = '\0';
    }

    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows(content_parent_);
}

void UsbSettingsScreen::save()
{
    settings::UsbSettings updated = settings::all().usb;
    std::strncpy(updated.default_password, password_shortcut_, sizeof(updated.default_password) - 1);
    updated.default_password[sizeof(updated.default_password) - 1] = '\0';
    updated.typing_order = typing_order_;
    updated.delay_before_typing_ms = delay_before_typing_ms_;
    updated.delay_between_chars_ms = delay_between_chars_ms_;
    updated.delay_between_fields_ms = delay_between_fields_ms_;

    if (settings::set_usb(updated)) {
        ESP_LOGI(TAG, "USB settings saved");
        manager().pop();
    } else {
        lv_label_set_text(status_label_, "Save failed");
    }
}

bool UsbSettingsScreen::on_input(InputAction action)
{
    if (mode_ == Mode::EditText) {
        const bool consumed = text_entry_.on_input(action);

        if (text_entry_.is_finished()) {
            exit_edit_password(true);
            return true;
        }
        if (!consumed) {
            exit_edit_password(false);
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
            return false; // pop, discarding unsaved changes

        default:
            return false;
    }
}

} // namespace ui::screens
