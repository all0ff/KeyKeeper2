#include "ui/screens/quick_screen.hpp"

#include "ui/screens/lock_screen.hpp"
#include "ui/localization.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"
#include "ui/screens/main_menu.hpp"

#include "security/lock_manager.hpp"
#include "settings/settings.hpp"
#include "usb/usb_service.hpp"
#include "wifi/wifi_service.hpp"

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

    lv_label_set_text(state_label_, locked ? i18n::tr(i18n::Key::LockedStatus) : i18n::tr(i18n::Key::UnlockedStatus));

    if (locked) {
        std::snprintf(footer_buf_, sizeof(footer_buf_), "%s", i18n::tr(i18n::Key::QuickFooterLockedFmt));
    } else {
        std::snprintf(footer_buf_, sizeof(footer_buf_), "%s", i18n::tr(i18n::Key::QuickFooterUnlockedFmt));
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
            // Deliberately NOT gated behind security::permission::check()
            // -- matches KeyKeeper 1.90's own "quick password without
            // PIN" feature exactly (see settings::UsbSettings::default_password's
            // doc comment for the trade-off this represents). Unlike
            // Print Password on a real vault entry (which stays gated
            // elsewhere), this is a single, separately-configured field
            // the person explicitly set up to be reachable without
            // unlocking.
            const char* password = settings::all().usb.default_password;

            if (password == nullptr || password[0] == '\0') {
                ESP_LOGI(TAG, "Password Shortcut is empty");
                lv_label_set_text(status_label_, i18n::tr(i18n::Key::PasswordShortcutEmpty));
                return true;
            }

            usb::type_string(password);
            lv_label_set_text(status_label_, usb::last_status());
            return true;
        }

        case InputAction::OkShort: {
            // Deliberately NOT gated behind security::permission::check()
            // -- this resolves the OPEN SPEC CONFLICT this file used to
            // flag (see the updated header comment): unlike Print
            // Password, this types the Web UI's network address, not any
            // stored secret. Confirmed against KeyKeeper 1.90's own
            // reference behavior, where the equivalent action (Main
            // button click while idle) worked identically whether or not
            // a PIN had been entered yet.
            const char* ip = wifi::ip_address();
            if (ip[0] == '\0') {
                ESP_LOGI(TAG, "Print URL: WiFi not connected");
                lv_label_set_text(status_label_, i18n::tr(i18n::Key::WifiNotConnected));
                return true;
            }

            // secret_word matches KeyKeeper 1.90's own "secretword"
            // feature -- see settings::SecuritySettings::secret_word's
            // doc comment. Empty means no prefix, same as 1.90.
            const settings::SecuritySettings& sec = settings::all().security;
            char url[96];
            if (sec.secret_word[0] != '\0') {
                std::snprintf(url, sizeof(url), "http://%s/%s/", ip, sec.secret_word);
            } else {
                std::snprintf(url, sizeof(url), "http://%s/", ip);
            }

            usb::type_string(url);
            lv_label_set_text(status_label_, usb::last_status());
            return true;
        }

        default:
            return false;
    }
}

} // namespace ui::screens
