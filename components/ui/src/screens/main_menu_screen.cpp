#include "ui/screens/main_menu_screen.hpp"

#include "ui/theme.hpp"

#include "esp_log.h"

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.main_menu";

// IconId::About stands in for "System" -- no dedicated icon exists
// for it (see ui/icon.hpp); revisit once the real icon set exists.
constexpr widgets::MenuItem ITEMS[] = {
    {"Vault", icon::IconId::Vault},
    {"Favorites", icon::IconId::Favorite},
    {"Categories", icon::IconId::Folder},
    {"Search", icon::IconId::Search},
    {"Settings", icon::IconId::Settings},
    {"Backup", icon::IconId::Backup},
    {"System", icon::IconId::About},
};
constexpr size_t ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

} // namespace

const char* MainMenuScreen::title() const
{
    return "Main Menu";
}

const char* MainMenuScreen::footer_hint() const
{
    return "OK  Open    BACK  Lock screen";
}

void MainMenuScreen::initialize(lv_obj_t* content_parent)
{
    menu_.init(content_parent, ITEMS, ITEM_COUNT);

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);
}

void MainMenuScreen::on_show()
{
    lv_label_set_text(status_label_, "");
}

bool MainMenuScreen::on_input(InputAction action)
{
    const widgets::MenuResult result = menu_.on_input(action);

    switch (result) {
        case widgets::MenuResult::Moved:
            lv_label_set_text(status_label_, "");
            return true;

        case widgets::MenuResult::Activated: {
            // PLACEHOLDER: none of the 7 sections have a screen yet --
            // see main_menu_screen.hpp. Swap each case for a real
            // manager().push(...) as its screen gets built.
            const char* name = ITEMS[menu_.selected_index()].label;
            ESP_LOGI(TAG, "%s selected -- screen not built yet", name);
            lv_label_set_text_fmt(status_label_, "%s: coming soon", name);
            return true;
        }

        case widgets::MenuResult::Ignored:
        default:
            return false; // e.g. BackShort -- default nav pops back to QuickScreen
    }
}

} // namespace ui::screens
