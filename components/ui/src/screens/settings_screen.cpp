#include "ui/screens/settings_screen.hpp"

#include "ui/screens/general_settings_screen.hpp"
#include "ui/screens/password_gen_settings_screen.hpp"
#include "ui/screens/security_settings_screen.hpp"
#include "ui/screens/system_info_screen.hpp"
#include "ui/screens/usb_settings_screen.hpp"
#include "ui/screens/wifi_settings_screen.hpp"
#include "ui/localization.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include <memory>

namespace ui::screens {

namespace {

constexpr lv_coord_t ITEM_Y_START = 4;
constexpr lv_coord_t ITEM_SPACING = 20;

const char* item_name(size_t index)
{
    if (i18n::language() == settings::Language::Russian) {
        switch (index) {
            case 0: return "Общие";
            case 1: return "USB";
            case 2: return "Безопасность";
            case 3: return "Система";
            case 4: return "Wi-Fi";
            case 5: return "Генератор паролей";
            default: return "";
        }
    }

    switch (index) {
        case 0: return "General";
        case 1: return "USB";
        case 2: return "Security";
        case 3: return "System";
        case 4: return "WiFi";
        case 5: return "Password Gen";
        default: return "";
    }
}

} // namespace

const char* SettingsScreen::title() const
{
    return i18n::tr(i18n::Key::Settings);
}

const char* SettingsScreen::footer_hint() const
{
    if (i18n::language() == settings::Language::Russian) {
        return "OK  Открыть    НАЗАД  Возврат";
    }
    return "OK  Open    BACK  Return";
}

void SettingsScreen::initialize(lv_obj_t* content_parent)
{
    for (size_t i = 0; i < ITEM_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(content_parent);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ITEM_Y_START + static_cast<lv_coord_t>(ITEM_SPACING * i));
        item_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render();
}

void SettingsScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render();
}

void SettingsScreen::render()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ITEM_COUNT; ++i) {
        const bool is_selected = (i == selected_);
        lv_obj_set_style_text_color(item_labels_[i], is_selected ? pal.accent : pal.primary_text, 0);
        lv_label_set_text_fmt(item_labels_[i], "%s%s", is_selected ? "> " : "", item_name(i));
    }

    if (item_labels_[selected_] != nullptr) {
        lv_obj_scroll_to_view(item_labels_[selected_], LV_ANIM_ON);
    }
}

void SettingsScreen::move_selection(int32_t delta)
{
    int32_t index = static_cast<int32_t>(selected_) + delta;
    const int32_t count = static_cast<int32_t>(ITEM_COUNT);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_ = static_cast<size_t>(index);

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render();
}

void SettingsScreen::activate()
{
    switch (static_cast<Item>(selected_)) {
        case Item::General:
            manager().push(std::make_unique<GeneralSettingsScreen>());
            return;

        case Item::Security:
            manager().push(std::make_unique<SecuritySettingsScreen>());
            return;

        case Item::Usb:
            manager().push(std::make_unique<UsbSettingsScreen>());
            return;

        case Item::System:
            manager().push(std::make_unique<SystemInfoScreen>());
            return;

        case Item::Wifi:
            manager().push(std::make_unique<WifiSettingsScreen>());
            return;

        case Item::PasswordGen:
            manager().push(std::make_unique<PasswordGenSettingsScreen>());
            return;
    }
}

bool SettingsScreen::on_input(InputAction action)
{
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
