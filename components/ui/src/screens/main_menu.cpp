#include "ui/screens/main_menu.hpp"

#include "ui/screens/about_screen.hpp"
#include "ui/screens/accounts_screen.hpp"
#include "ui/screens/backup_screen.hpp"
#include "ui/screens/settings_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/lock_manager.hpp"

#include "esp_log.h"

#include <cstdio>
#include <memory>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.main_menu";

constexpr const char* ITEM_NAMES[] = {
    "Accounts",
    "Settings",
    "Backup",
    "Lock",
    "About",
};

constexpr lv_coord_t FIRST_ITEM_Y = 8;
constexpr lv_coord_t ITEM_SPACING = 20;

} // namespace

const char* MainMenu::title() const
{
    return "Main Menu";
}

const char* MainMenu::footer_hint() const
{
    return "ROTATE  Select    OK  Open    BACK  Return";
}

void MainMenu::initialize(lv_obj_t* content_parent)
{
    root_ = content_parent;

    const theme::Palette& pal = theme::current();

    for (uint8_t i = 0; i < ITEM_COUNT; ++i) {
        item_labels_[i] = lv_label_create(content_parent);

        lv_obj_set_style_text_color(
            item_labels_[i],
            pal.primary_text,
            0
        );

        lv_label_set_text(
            item_labels_[i],
            ITEM_NAMES[i]
        );

        lv_obj_align(
            item_labels_[i],
            LV_ALIGN_TOP_MID,
            0,
            FIRST_ITEM_Y + (ITEM_SPACING * i)
        );
    }

    status_label_ = lv_label_create(content_parent);

    lv_obj_set_style_text_color(
        status_label_,
        pal.secondary_text,
        0
    );

    lv_label_set_text(status_label_, "");

    lv_obj_align(
        status_label_,
        LV_ALIGN_BOTTOM_MID,
        0,
        -2
    );

    refresh();
}

void MainMenu::on_show()
{
    /*
     * A MainMenu instance is normally shown immediately after it is
     * created by LockScreen::try_unlock(). Resetting selection here
     * keeps the menu deterministic whenever it becomes active again.
     */
    selected_ = Item::Accounts;

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }

    refresh();
}

bool MainMenu::on_input(InputAction action)
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
            /*
             * Returning from MainMenu to QuickScreen is allowed.
             * QuickScreen remains underneath MainMenu in the stack.
             */
            return false;

        case InputAction::BackLong:
            /*
             * Long BACK is the explicit "lock now" action.
             *
             * security::lock::lock() publishes DeviceLocked and ends
             * the current session. Navigation back to QuickScreen
             * happens via ui.cpp's security::lock callback
             * (on_lock_state_changed -> reset_to_root()), not an
             * explicit pop() here -- that callback covers every path
             * to Locked uniformly.
             */
            security::lock::lock();

            ESP_LOGI(TAG, "Device locked from Main Menu");
            return true;

        default:
            return false;
    }
}

void MainMenu::move_selection(int8_t delta)
{
    int index = static_cast<int>(selected_);
    index += delta;

    if (index < 0) {
        index = ITEM_COUNT - 1;
    }

    if (index >= ITEM_COUNT) {
        index = 0;
    }

    selected_ = static_cast<Item>(index);

    refresh();
}

void MainMenu::refresh()
{
    const theme::Palette& pal = theme::current();

    const uint8_t selected_index =
        static_cast<uint8_t>(selected_);

    for (uint8_t i = 0; i < ITEM_COUNT; ++i) {
        if (item_labels_[i] == nullptr) {
            continue;
        }

        if (i == selected_index) {
            lv_obj_set_style_text_color(
                item_labels_[i],
                pal.accent,
                0
            );

            lv_label_set_text_fmt(
                item_labels_[i],
                "> %s",
                ITEM_NAMES[i]
            );
        } else {
            lv_obj_set_style_text_color(
                item_labels_[i],
                pal.primary_text,
                0
            );

            lv_label_set_text(
                item_labels_[i],
                ITEM_NAMES[i]
            );
        }
    }

    if (item_labels_[selected_index] != nullptr) {
        lv_obj_scroll_to_view(item_labels_[selected_index], LV_ANIM_ON);
    }
}

void MainMenu::activate()
{
    switch (selected_) {
        case Item::Accounts:
            manager().push(std::make_unique<AccountsScreen>());
            ESP_LOGI(TAG, "Accounts selected");
            break;

        case Item::Settings:
            manager().push(std::make_unique<SettingsScreen>());
            ESP_LOGI(TAG, "Settings selected");
            break;

        case Item::Backup:
            manager().push(std::make_unique<BackupScreen>());
            ESP_LOGI(TAG, "Backup selected");
            break;

        case Item::Lock:
            // Navigation back to QuickScreen happens via ui.cpp's
            // security::lock callback now -- see BackLong's identical
            // comment above.
            security::lock::lock();
            ESP_LOGI(TAG, "Device locked from Main Menu");
            break;

        case Item::About:
            manager().push(std::make_unique<AboutScreen>());
            ESP_LOGI(TAG, "About selected");
            break;

        case Item::Count:
            break;
    }
}

} // namespace ui::screens
