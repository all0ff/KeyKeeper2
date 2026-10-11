#include "ui/screens/power_settings_screen.hpp"

#include "ui/localization.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "settings/settings.hpp"

#include "esp_log.h"

#include <cstddef>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.power_settings";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

constexpr uint16_t MINUTES[] = {0, 1, 2, 5, 10, 15, 30, 60};
constexpr size_t MINUTES_COUNT = sizeof(MINUTES) / sizeof(MINUTES[0]);

bool ru()
{
    return i18n::language() == settings::Language::Russian;
}

size_t minutes_index(uint16_t value)
{
    size_t best = 0;
    for (size_t i = 0; i < MINUTES_COUNT; ++i) {
        if (MINUTES[i] <= value) best = i;
    }
    return best;
}

uint16_t step_minutes(uint16_t value, int32_t delta)
{
    int32_t i = static_cast<int32_t>(minutes_index(value)) + delta;
    if (i < 0) i = 0;
    if (i >= static_cast<int32_t>(MINUTES_COUNT)) i = static_cast<int32_t>(MINUTES_COUNT) - 1;
    return MINUTES[i];
}

void minutes_text(char* out, size_t n, uint16_t m)
{
    if (m == 0) {
        std::snprintf(out, n, "%s", i18n::tr(i18n::Key::Off));
    } else {
        std::snprintf(out, n, ru() ? "%u мин" : "%u min", static_cast<unsigned>(m));
    }
}

} // namespace

const char* PowerSettingsScreen::title() const
{
    return ru() ? "Экономия энергии" : "Energy saving";
}

const char* PowerSettingsScreen::footer_hint() const
{
    if (mode_ == Mode::Adjust) {
        return i18n::tr(i18n::Key::OkBackConfirm);
    }
    return i18n::tr(i18n::Key::OkOpenBackCancel);
}

void PowerSettingsScreen::initialize(lv_obj_t* content_parent)
{
    work_ = settings::all().power;

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

void PowerSettingsScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render();
}

void PowerSettingsScreen::render()
{
    const theme::Palette& pal = theme::current();
    char value[24];

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool is_selected = (i == selected_row_);
        const bool is_adjusting = is_selected && mode_ == Mode::Adjust;
        lv_obj_set_style_text_color(
            row_labels_[i], is_adjusting ? pal.warning : (is_selected ? pal.accent : pal.primary_text), 0);

        const char* prefix = is_selected ? "> " : "";

        switch (static_cast<Row>(i)) {
            case Row::WifiOff:
                minutes_text(value, sizeof value, work_.wifi_idle_off_min);
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix,
                                      ru() ? "Wi-Fi выкл. через" : "Wi-Fi off after", value);
                break;
            case Row::BleOff:
                minutes_text(value, sizeof value, work_.ble_idle_off_min);
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix,
                                      ru() ? "Донгл выкл. через" : "Dongle off after", value);
                break;
            case Row::CpuIdle:
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix,
                                      ru() ? "CPU в простое" : "CPU at idle",
                                      work_.cpu_idle_low ? "80 MHz" : "240 MHz");
                break;
            case Row::Battery: {
                const char* v = ru() ? "нет" : "off";
                if (work_.battery_display == settings::BatteryDisplay::Icon) v = ru() ? "значок" : "icon";
                if (work_.battery_display == settings::BatteryDisplay::Percent) v = ru() ? "проценты" : "percent";
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, ru() ? "Заряд" : "Battery", v);
                break;
            }
            case Row::Save:
                lv_label_set_text_fmt(row_labels_[i], "%s%s", prefix, i18n::tr(i18n::Key::Save));
                break;
        }
    }

    if (row_labels_[selected_row_] != nullptr) {
        lv_obj_scroll_to_view(row_labels_[selected_row_], LV_ANIM_ON);
    }
}

void PowerSettingsScreen::move_selection(int32_t delta)
{
    int32_t index = static_cast<int32_t>(selected_row_) + delta;
    const int32_t count = static_cast<int32_t>(ROW_COUNT);
    if (index < 0) index = count - 1;
    if (index >= count) index = 0;
    selected_row_ = static_cast<size_t>(index);

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render();
}

void PowerSettingsScreen::adjust_value(int32_t delta)
{
    switch (static_cast<Row>(selected_row_)) {
        case Row::WifiOff:
            work_.wifi_idle_off_min = step_minutes(work_.wifi_idle_off_min, delta);
            break;
        case Row::BleOff:
            work_.ble_idle_off_min = step_minutes(work_.ble_idle_off_min, delta);
            break;
        case Row::CpuIdle:
            work_.cpu_idle_low = !work_.cpu_idle_low;
            break;
        case Row::Battery: {
            int32_t v = static_cast<int32_t>(work_.battery_display) + delta;
            if (v < 0) v = 2;
            if (v > 2) v = 0;
            work_.battery_display = static_cast<settings::BatteryDisplay>(v);
            break;
        }
        case Row::Save:
            break;
    }
    render();
}

void PowerSettingsScreen::begin_adjust()
{
    snapshot_ = work_;
    mode_ = Mode::Adjust;
    render();
}

void PowerSettingsScreen::end_adjust(bool commit)
{
    if (!commit) {
        work_ = snapshot_;
    }
    mode_ = Mode::Browse;
    render();
}

void PowerSettingsScreen::activate()
{
    if (static_cast<Row>(selected_row_) == Row::Save) {
        save();
        return;
    }
    begin_adjust();
}

void PowerSettingsScreen::save()
{
    if (settings::set_power(work_)) {
        ESP_LOGI(TAG, "Energy settings saved: wifi_off=%u ble_off=%u cpu_low=%d battery=%d",
                 static_cast<unsigned>(work_.wifi_idle_off_min), static_cast<unsigned>(work_.ble_idle_off_min),
                 work_.cpu_idle_low ? 1 : 0, static_cast<int>(work_.battery_display));
        manager().pop();
    } else {
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::SaveFailed));
    }
}

bool PowerSettingsScreen::on_input(InputAction action)
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
                end_adjust(true);
                return true;
            case InputAction::BackShort:
                end_adjust(false);
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
            return false;
        default:
            return false;
    }
}

} // namespace ui::screens
