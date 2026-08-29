#include "ui/screens/lock_screen.hpp"

#include "ui/screens/main_menu.hpp"
#include "ui/screens/setup_pin_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/lock_manager.hpp"
#include "security/pin_manager.hpp"
#include "settings/settings.hpp"

#include "esp_log.h"

#include <cstdio>
#include <memory>

namespace ui::screens {

namespace {
constexpr char TAG[] = "ui.lock_screen";
}

const char* LockScreen::title() const
{
    return "Unlock";
}

const char* LockScreen::footer_hint() const
{
    return "BACK  Erase digit / Cancel";
}

void LockScreen::initialize(lv_obj_t* content_parent)
{
    widgets::PinEntry::Config cfg{};
    cfg.length = settings::all().security.pin_length;
    pin_entry_.init(content_parent, cfg);

    const theme::Palette& pal = theme::current();

    message_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(message_label_, pal.error, 0);
    lv_label_set_text(message_label_, "");
    lv_obj_align(message_label_, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void LockScreen::on_show()
{
    pin_entry_.reset();
    lv_label_set_text(message_label_, "");
}

bool LockScreen::on_input(InputAction action)
{
    const bool consumed = pin_entry_.on_input(action);

    if (consumed && pin_entry_.is_complete()) {
        try_unlock();
    }

    return consumed;
}

void LockScreen::try_unlock()
{
    const security::pin::VerifyResult result = security::lock::unlock(pin_entry_.pin());

    pin_entry_.reset();

    switch (result) {
        case security::pin::VerifyResult::Success:
            ESP_LOGI(TAG, "Unlock successful");
            manager().replace(std::make_unique<MainMenu>());
            return;

        case security::pin::VerifyResult::WrongPin: {
            const uint8_t remaining = security::pin::attempts_remaining();
            char buf[32];
            std::snprintf(buf, sizeof(buf), "Wrong PIN, %u left", static_cast<unsigned>(remaining));
            lv_label_set_text(message_label_, buf);
            ESP_LOGI(TAG, "Wrong PIN, %u attempts left", static_cast<unsigned>(remaining));
            return;
        }

        case security::pin::VerifyResult::LockedOut:
            lv_label_set_text(message_label_, "Locked out, try later");
            ESP_LOGI(TAG, "Unlock denied: locked out");
            return;

        case security::pin::VerifyResult::NoPinSet:
            // PIN was never configured -- redirect to setup flow
            ESP_LOGI(TAG, "No PIN set, redirecting to SetupPinScreen");
            manager().replace(std::make_unique<SetupPinScreen>());
            return;
    }
}

} // namespace ui::screens
