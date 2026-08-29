#include "ui/screens/general_settings_screen.hpp"

#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "display/display.hpp"
#include "settings/settings.hpp"

#include "esp_log.h"

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.general_settings";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

constexpr int32_t BRIGHTNESS_STEP = 5;
constexpr int32_t TIMEOUT_STEP_S = 5;
constexpr uint32_t TIMEOUT_MIN_S = 5;
constexpr uint32_t TIMEOUT_MAX_S = 300; // placeholder range, not spec'd anywhere

} // namespace

const char* GeneralSettingsScreen::title() const
{
    return "General";
}

const char* GeneralSettingsScreen::footer_hint() const
{
    if (mode_ == Mode::Adjust) {
        return "ROTATE  Change    OK/BACK  Confirm";
    }
    return "OK  Open    BACK  Cancel";
}

void GeneralSettingsScreen::initialize(lv_obj_t* content_parent)
{
    const settings::GeneralSettings& g = settings::all().general;
    language_ = g.language;
    theme_ = g.theme;
    brightness_ = g.display_brightness;
    screen_timeout_s_ = g.display_off_timeout_s;
    original_brightness_ = g.display_brightness;
    saved_ = false;

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(content_parent);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        row_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render();
}

void GeneralSettingsScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
}

void GeneralSettingsScreen::on_hide()
{
    // Undo the live brightness preview if the user leaves without
    // saving -- otherwise the display would be left at a brightness
    // that was never actually persisted.
    if (!saved_) {
        display::set_brightness(original_brightness_);
    }
}

void GeneralSettingsScreen::render()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool is_selected = (i == selected_row_);
        const bool is_adjusting = is_selected && mode_ == Mode::Adjust;
        lv_obj_set_style_text_color(
            row_labels_[i], is_adjusting ? pal.warning : (is_selected ? pal.accent : pal.primary_text), 0);

        const char* prefix = is_selected ? "> " : "";

        switch (static_cast<Row>(i)) {
            case Row::Language:
                lv_label_set_text_fmt(row_labels_[i], "%sLanguage: %s", prefix,
                                       language_ == settings::Language::English ? "English" : "Russian");
                break;
            case Row::Theme:
                // Only Theme::Dark exists today -- see the header
                // comment, this is a placeholder for a future second
                // theme, not a live-adjustable value right now.
                lv_label_set_text_fmt(row_labels_[i], "%sTheme: Dark", prefix);
                break;
            case Row::Brightness:
                lv_label_set_text_fmt(row_labels_[i], "%sBrightness: %u%%", prefix,
                                       static_cast<unsigned>(brightness_));
                break;
            case Row::ScreenTimeout:
                lv_label_set_text_fmt(row_labels_[i], "%sScreen Timeout: %lus", prefix,
                                       static_cast<unsigned long>(screen_timeout_s_));
                break;
            case Row::Save:
                lv_label_set_text_fmt(row_labels_[i], "%sSave", prefix);
                break;
        }
    }
}

void GeneralSettingsScreen::move_selection(int32_t delta)
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
    render();
}

void GeneralSettingsScreen::adjust_value(int32_t delta)
{
    switch (static_cast<Row>(selected_row_)) {
        case Row::Language:
            language_ = (language_ == settings::Language::English) ? settings::Language::Russian
                                                                     : settings::Language::English;
            break;

        case Row::Theme:
            // No-op -- only one value exists.
            break;

        case Row::Brightness: {
            int32_t value = static_cast<int32_t>(brightness_) + delta * BRIGHTNESS_STEP;
            if (value < 0) {
                value = 0;
            }
            if (value > 100) {
                value = 100;
            }
            brightness_ = static_cast<uint8_t>(value);
            display::set_brightness(brightness_); // live preview
            break;
        }

        case Row::ScreenTimeout: {
            int32_t value = static_cast<int32_t>(screen_timeout_s_) + delta * TIMEOUT_STEP_S;
            if (value < static_cast<int32_t>(TIMEOUT_MIN_S)) {
                value = static_cast<int32_t>(TIMEOUT_MIN_S);
            }
            if (value > static_cast<int32_t>(TIMEOUT_MAX_S)) {
                value = static_cast<int32_t>(TIMEOUT_MAX_S);
            }
            screen_timeout_s_ = static_cast<uint32_t>(value);
            break;
        }

        case Row::Save:
            break;
    }

    render();
}

void GeneralSettingsScreen::activate()
{
    if (static_cast<Row>(selected_row_) == Row::Save) {
        save();
        return;
    }

    mode_ = Mode::Adjust;
    render();
}

void GeneralSettingsScreen::save()
{
    settings::GeneralSettings updated = settings::all().general;
    updated.language = language_;
    updated.theme = theme_;
    updated.display_brightness = brightness_;
    updated.display_off_timeout_s = screen_timeout_s_;

    if (settings::set_general(updated)) {
        saved_ = true;
        ESP_LOGI(TAG, "General settings saved");
        manager().pop();
    } else {
        lv_label_set_text(status_label_, "Save failed");
    }
}

bool GeneralSettingsScreen::on_input(InputAction action)
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
                render();
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
            return false; // pop, discarding unsaved changes (brightness reverted in on_hide())

        default:
            return false;
    }
}

} // namespace ui::screens
