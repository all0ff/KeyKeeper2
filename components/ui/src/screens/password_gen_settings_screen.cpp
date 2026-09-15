#include "ui/screens/password_gen_settings_screen.hpp"

#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "password_gen/password_gen.hpp"
#include "settings/settings.hpp"
#include "usb/usb_service.hpp"

#include "esp_log.h"

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.password_gen_settings";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

constexpr int32_t LENGTH_STEP = 1;

} // namespace

const char* PasswordGenSettingsScreen::title() const
{
    return "Password Gen";
}

const char* PasswordGenSettingsScreen::footer_hint() const
{
    if (mode_ == Mode::Adjust) {
        return "ROTATE  Change    OK/BACK  Confirm";
    }
    return "OK  Open    BACK  Cancel";
}

void PasswordGenSettingsScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    const settings::PasswordGenSettings& p = settings::all().password_gen;
    length_ = p.length;
    uppercase_ = p.include_uppercase;
    lowercase_ = p.include_lowercase;
    digits_ = p.include_digits;
    symbols_ = p.include_symbols;

    build_rows(content_parent);
}

void PasswordGenSettingsScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
}

void PasswordGenSettingsScreen::build_rows(lv_obj_t* parent)
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

void PasswordGenSettingsScreen::render_rows()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool is_selected = (i == selected_row_);
        const bool is_adjusting = is_selected && mode_ == Mode::Adjust;
        lv_obj_set_style_text_color(
            row_labels_[i], is_adjusting ? pal.warning : (is_selected ? pal.accent : pal.primary_text), 0);

        const char* prefix = is_selected ? "> " : "";

        switch (static_cast<Row>(i)) {
            case Row::Length:
                lv_label_set_text_fmt(row_labels_[i], "%sLength: %u", prefix, static_cast<unsigned>(length_));
                break;
            case Row::Uppercase:
                lv_label_set_text_fmt(row_labels_[i], "%sUppercase (A-Z): %s", prefix, uppercase_ ? "on" : "off");
                break;
            case Row::Lowercase:
                lv_label_set_text_fmt(row_labels_[i], "%sLowercase (a-z): %s", prefix, lowercase_ ? "on" : "off");
                break;
            case Row::Digits:
                lv_label_set_text_fmt(row_labels_[i], "%sDigits (0-9): %s", prefix, digits_ ? "on" : "off");
                break;
            case Row::Symbols:
                lv_label_set_text_fmt(row_labels_[i], "%sSymbols (!@#...): %s", prefix, symbols_ ? "on" : "off");
                break;
            case Row::GenerateAndType:
                lv_label_set_text_fmt(row_labels_[i], "%sGenerate & Type", prefix);
                break;
            case Row::Save:
                lv_label_set_text_fmt(row_labels_[i], "%sSave", prefix);
                break;
        }
    }

    if (row_labels_[selected_row_] != nullptr) {
        lv_obj_scroll_to_view(row_labels_[selected_row_], LV_ANIM_ON);
    }
}

void PasswordGenSettingsScreen::move_selection(int32_t delta)
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

void PasswordGenSettingsScreen::adjust_value(int32_t delta)
{
    switch (static_cast<Row>(selected_row_)) {
        case Row::Length: {
            int32_t value = static_cast<int32_t>(length_) + delta * LENGTH_STEP;
            if (value < static_cast<int32_t>(password_gen::MIN_LENGTH)) {
                value = password_gen::MIN_LENGTH;
            }
            if (value > static_cast<int32_t>(password_gen::MAX_LENGTH)) {
                value = password_gen::MAX_LENGTH;
            }
            length_ = static_cast<uint8_t>(value);
            break;
        }

        case Row::Uppercase: uppercase_ = !uppercase_; break;
        case Row::Lowercase: lowercase_ = !lowercase_; break;
        case Row::Digits:    digits_ = !digits_; break;
        case Row::Symbols:   symbols_ = !symbols_; break;

        case Row::GenerateAndType:
        case Row::Save:
            break;
    }

    render_rows();
}

void PasswordGenSettingsScreen::activate()
{
    if (static_cast<Row>(selected_row_) == Row::Save) {
        save();
        return;
    }

    if (static_cast<Row>(selected_row_) == Row::GenerateAndType) {
        generate_and_type();
        return;
    }

    mode_ = Mode::Adjust;
    render_rows();
}

void PasswordGenSettingsScreen::generate_and_type()
{
    // Uses the CURRENT in-memory values, not necessarily what's been
    // Saved -- see this screen's own header comment.
    settings::PasswordGenSettings cfg;
    cfg.length = length_;
    cfg.include_uppercase = uppercase_;
    cfg.include_lowercase = lowercase_;
    cfg.include_digits = digits_;
    cfg.include_symbols = symbols_;

    char buf[password_gen::MAX_LENGTH + 1];
    if (!password_gen::generate(cfg, buf, sizeof(buf))) {
        lv_label_set_text(status_label_, "Enable at least one character type");
        return;
    }

    usb::type_string(buf);
    lv_label_set_text(status_label_, usb::last_status());
}

void PasswordGenSettingsScreen::save()
{
    if (!uppercase_ && !lowercase_ && !digits_ && !symbols_) {
        // Nothing to draw from -- password_gen::generate() would
        // always fail with all four classes off. Refuse to save this
        // combination rather than silently persisting a setting that
        // can never actually generate anything.
        lv_label_set_text(status_label_, "Enable at least one character type");
        return;
    }

    settings::PasswordGenSettings updated;
    updated.length = length_;
    updated.include_uppercase = uppercase_;
    updated.include_lowercase = lowercase_;
    updated.include_digits = digits_;
    updated.include_symbols = symbols_;

    if (settings::set_password_gen(updated)) {
        ESP_LOGI(TAG, "Password generator settings saved");
        manager().pop();
    } else {
        lv_label_set_text(status_label_, "Save failed");
    }
}

bool PasswordGenSettingsScreen::on_input(InputAction action)
{
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
