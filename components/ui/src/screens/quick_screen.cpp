#include "ui/screens/quick_screen.hpp"

#include "ui/screens/lock_screen.hpp"
#include "ui/screens/main_menu.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/lock_manager.hpp"
#include "security/permission_manager.hpp"

#include "esp_log.h"

#include <cstdio>
#include <memory>

namespace ui::screens {

namespace {
constexpr char TAG[] = "ui.quick_screen";
}

const char* QuickScreen::title() const
{
    return "KeyKeeper2";
}

const char* QuickScreen::footer_hint() const
{
    return footer_buf_;
}

void QuickScreen::initialize(lv_obj_t* content_parent)
{
    const theme::Palette& pal = theme::current();

    state_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(state_label_, pal.primary_text, 0);
    lv_obj_align(state_label_, LV_ALIGN_CENTER, 0, -10);

    status_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 14);

    refresh();
}

void QuickScreen::on_show()
{
    lv_label_set_text(status_label_, "");
    refresh();
}

void QuickScreen::refresh()
{
    const bool locked = security::lock::state() == security::lock::State::Locked;

    lv_label_set_text(state_label_, locked ? "Locked" : "Unlocked");

    if (locked) {
        std::snprintf(footer_buf_, sizeof(footer_buf_), "BACK  Unlock   OK  Print URL");
    } else {
        std::snprintf(footer_buf_, sizeof(footer_buf_), "OK  Print URL");
    }
}

bool QuickScreen::on_input(InputAction action)
{
    const bool locked = security::lock::state() == security::lock::State::Locked;

    switch (action) {
        case InputAction::BackShort:
            if (locked) {
                manager().push(std::make_unique<LockScreen>());
                return true; // suppress default pop while locked
            }

            manager().push(std::make_unique<MainMenu>());
            return true; // QuickScreen is always stack-bottom; pop() is a no-op anyway

        case InputAction::BackLong: {
            const security::permission::Result perm =
                security::permission::check(security::permission::Operation::PrintPassword);
            if (perm == security::permission::Result::Allowed) {
                // PLACEHOLDER: no USB HID OutputChannel implementation yet.
                ESP_LOGI(TAG, "Print Password: permission OK, USB HID not implemented yet");
                lv_label_set_text(status_label_, "USB typing: coming soon");
            } else {
                ESP_LOGI(TAG, "Print Password denied (%d)", static_cast<int>(perm));
                lv_label_set_text(status_label_, locked ? "Unlock first" : "Not allowed");
            }
            return true;
        }

        case InputAction::OkShort: {
            // See quick_screen.hpp: no dedicated PrintUrl permission
            // operation exists yet, reusing PrintPassword's gate as a
            // placeholder.
            const security::permission::Result perm =
                security::permission::check(security::permission::Operation::PrintPassword);
            if (perm == security::permission::Result::Allowed) {
                ESP_LOGI(TAG, "Print URL: permission OK, USB HID not implemented yet");
                lv_label_set_text(status_label_, "USB typing: coming soon");
            } else {
                ESP_LOGI(TAG, "Print URL denied (%d)", static_cast<int>(perm));
                lv_label_set_text(status_label_, locked ? "Unlock first" : "Not allowed");
            }
            return true;
        }

        default:
            return false;
    }
}

} // namespace ui::screens
