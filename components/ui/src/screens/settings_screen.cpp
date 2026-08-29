#include "ui/screens/settings_screen.hpp"

#include "ui/screens/general_settings_screen.hpp"
#include "ui/screens/security_settings_screen.hpp"
#include "ui/screens/system_info_screen.hpp"
#include "ui/screens/usb_settings_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include <memory>

namespace ui::screens {

namespace {

constexpr lv_coord_t ITEM_Y_START = 4;
constexpr lv_coord_t ITEM_SPACING = 20;

constexpr const char* ITEM_NAMES[] = {"General", "USB", "Security", "System"};

} // namespace

const char* SettingsScreen::title() const
{
    return "Settings";
}

const char* SettingsScreen::footer_hint() const
{
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
        lv_label_set_text_fmt(item_labels_[i], "%s%s", is_selected ? "> " : "", ITEM_NAMES[i]);
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
            return false; // pop back to MainMenu

        default:
            return false;
    }
}

} // namespace ui::screens
