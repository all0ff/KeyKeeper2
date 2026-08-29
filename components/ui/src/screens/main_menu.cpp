#include "ui/screens/main_menu.hpp"

#include "ui/screens/settings_screen.hpp"
#include "ui/screens/vault_list_screen.hpp"
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
    "Vault",
    "Settings",
    "Lock",
    "About",
};

constexpr lv_coord_t FIRST_ITEM_Y = 8;
constexpr lv_coord_t ITEM_SPACING = 26;

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
    selected_ = Item::Vault;

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
             * security::lock::lock() publishes DeviceLocked and
             * ends the current session. The UI will receive the
             * corresponding state through the existing security/UI
             * flow. For now we return to the QuickScreen immediately.
             */
            security::lock::lock();

            ESP_LOGI(TAG, "Device locked from Main Menu");

            manager().pop();
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
}

void MainMenu::activate()
{
    switch (selected_) {
        case Item::Vault:
            /*
             * VaultListScreen -- see docs/GUI.md section 10.
             */
            manager().push(std::make_unique<VaultListScreen>());

            ESP_LOGI(TAG, "Vault selected");
            break;

        case Item::Settings:
            /*
             * SettingsScreen -- see docs/GUI.md section 14.
             */
            manager().push(std::make_unique<SettingsScreen>());

            ESP_LOGI(TAG, "Settings selected");
            break;

        case Item::Lock:
            security::lock::lock();

            ESP_LOGI(TAG, "Device locked from Main Menu");

            /*
             * MainMenu is removed from the stack. QuickScreen becomes
             * active and reflects the Locked security state.
             */
            manager().pop();
            break;

        case Item::About:
            /*
             * AboutScreen will be added later.
             */
            lv_label_set_text(
                status_label_,
                "About: coming soon"
            );

            ESP_LOGI(TAG, "About selected");
            break;

        case Item::Count:
            break;
    }
}

} // namespace ui::screens