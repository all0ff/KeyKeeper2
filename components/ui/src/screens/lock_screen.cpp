#include "ui/screens/lock_screen.hpp"

#include "ui/screens/main_menu.hpp"
#include "ui/screens/setup_pin_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/lock_manager.hpp"
#include "security/pin_manager.hpp"
#include "settings/settings.hpp"
#include "vault/vault_repository.hpp"

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
    if (checking_) {
        return "Checking...";
    }
    return "ROTATE Digit  OK Next  Hold OK Done  BACK Erase";
}

void LockScreen::initialize(lv_obj_t* content_parent)
{
    // Box count = settings::all().security.pin_length exactly, at the
    // project owner's explicit request (better UX: matches what you
    // actually typed when you set your PIN, not a padded 4-6 range).
    // Confirmed aware this reintroduces the risk the flexible range
    // was added to prevent (if this setting ever disagrees with the
    // ACTUAL stored PIN's length, PinEntry hard-caps entry at
    // cfg.length, making the real PIN impossible to type) -- but the
    // specific cause that triggered that once (a settings-storage
    // layout change silently reverting to defaults) is a one-time
    // migration hazard, already behind this project, not a standing
    // risk. verify() itself no longer requires this to match anyway
    // (see pin_manager.cpp's pin_format_ok()) -- only entry is capped
    // by it, not verification.
    widgets::PinEntry::Config cfg{};
    cfg.length = settings::all().security.pin_length;
    if (cfg.length < 4 || cfg.length > 6) {
        cfg.length = 6;
    }
    cfg.min_length = cfg.length;
    cfg.finish_on_short = true;

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
    checking_ = false;
    lv_label_set_text(message_label_, "");
}

bool LockScreen::on_input(InputAction action)
{
    if (checking_) {
        // Swallow everything while a check is in flight -- see
        // lock_screen.hpp's file comment. AsyncPinCheck itself would
        // be safe even if we let BackShort pop this screen mid-check,
        // but there's no reason to.
        return true;
    }

    const bool consumed = pin_entry_.on_input(action);

    if (consumed && pin_entry_.is_complete()) {
        try_unlock();
    }

    return consumed;
}

void LockScreen::try_unlock()
{
    checking_ = true;
    lv_label_set_text(message_label_, "Checking...");

    // pin_entry_.pin() is copied internally by AsyncPinCheck::start(),
    // so resetting pin_entry_ right after this call is safe -- same
    // "never keep entered digits around longer than needed" rule
    // already followed everywhere else PinEntry is used.
    async_check_.start(AsyncPinCheck::Kind::Unlock, pin_entry_.pin(), &LockScreen::on_check_done, this);
    pin_entry_.reset();
}

void LockScreen::on_check_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<LockScreen*>(ctx)->handle_result(result);
}

void LockScreen::handle_result(security::pin::VerifyResult result)
{
    checking_ = false;

    switch (result) {
        case security::pin::VerifyResult::DuressTriggered:
            // Wipes the vault ONLY -- deliberately NOT the PIN (see
            // pin_manager.hpp's DuressTriggered comment: the device
            // must go on behaving completely normally afterward, same
            // PIN still works, just an empty vault). Falls straight
            // through to the exact same path as a real Success --
            // same log line even -- no visible difference on screen.
            ESP_LOGW(TAG, "Duress PIN triggered -- wiping vault silently");
            vault::repository::wipe();
            [[fallthrough]];

        case security::pin::VerifyResult::Success:
            ESP_LOGI(TAG, "Unlock successful");
            manager().replace(std::make_unique<MainMenu>());
            return;

        case security::pin::VerifyResult::WrongPin: {
            const uint8_t remaining = security::pin::attempts_remaining();
            char buf[48];
            if (remaining > 0) {
                std::snprintf(buf, sizeof(buf), "Wrong PIN, %u left", static_cast<unsigned>(remaining));
            } else {
                // Past the first lockout threshold -- attempts_remaining()
                // saturates at 0 here, which used to give the user no
                // indication at all that they're getting closer to an
                // irreversible automatic wipe. Escalate explicitly.
                const uint8_t until_wipe = security::pin::attempts_until_wipe();
                std::snprintf(buf, sizeof(buf), "Wrong PIN! %u attempts until vault wipe",
                              static_cast<unsigned>(until_wipe));
            }
            lv_label_set_text(message_label_, buf);
            ESP_LOGW(TAG, "Wrong PIN, %u until wipe", static_cast<unsigned>(security::pin::attempts_until_wipe()));
            return;
        }

        case security::pin::VerifyResult::LockedOut:
            lv_label_set_text(message_label_, "Locked out, try later");
            ESP_LOGI(TAG, "Unlock denied: locked out");
            return;

        case security::pin::VerifyResult::WipeRequired: {
            ESP_LOGW(TAG, "Automatic wipe requested after PIN failure threshold");

            // Remove the persistent vault first. Only erase the PIN after
            // the vault wipe has completed successfully.
            const bool vault_wiped = vault::repository::wipe();
            const bool pin_wiped = vault_wiped && security::pin::wipe();

            if (!pin_wiped) {
                lv_label_set_text(message_label_, "Wipe error");
                ESP_LOGE(TAG, "Automatic wipe failed (vault=%d, pin=%d)",
                         vault_wiped ? 1 : 0, pin_wiped ? 1 : 0);
                return;
            }

            // Return to the normal first-launch setup flow. Do not expose
            // a separate "Vault wiped" state or message.
            manager().replace(std::make_unique<SetupPinScreen>());
            return;
        }

        case security::pin::VerifyResult::NoPinSet:
            // PIN was never configured -- redirect to setup flow
            ESP_LOGI(TAG, "No PIN set, redirecting to SetupPinScreen");
            manager().replace(std::make_unique<SetupPinScreen>());
            return;
    }
}

} // namespace ui::screens
