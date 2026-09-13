#include "ui/screens/setup_pin_screen.hpp"

#include "ui/screens/main_menu.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/lock_manager.hpp"
#include "security/pin_manager.hpp"
#include "settings/settings.hpp"

#include "esp_log.h"

#include <cstring>
#include <memory>

namespace ui::screens {

namespace {
constexpr char TAG[] = "ui.setup_pin";
}

const char* SetupPinScreen::title() const
{
    return "Setup PIN";
}

const char* SetupPinScreen::footer_hint() const
{
    if (checking_) {
        return "Checking...";
    }
    switch (stage_) {
        case Stage::EnterNew:
            return "ROTATE Digit OK Next Hold OK Done BACK Erase";
        case Stage::Confirm:
            return "ROTATE Digit OK Next Hold OK Done BACK Erase";
        case Stage::MismatchError:
            return "OK  Retry";
        default:
            return "";
    }
}

void SetupPinScreen::initialize(lv_obj_t* content_parent)
{
    const theme::Palette& pal = theme::current();

    // Prompt label ("Enter new PIN" / "Confirm PIN")
    prompt_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(prompt_label_, pal.primary_text, 0);
    lv_obj_align(prompt_label_, LV_ALIGN_TOP_MID, 0, 8);

    // PinEntry widget
    widgets::PinEntry::Config cfg{};

    cfg.length = 6;
    cfg.min_length = 4;
    cfg.mask_confirmed = true;

    pin_entry_.init(content_parent, cfg);
    lv_obj_align(pin_entry_.root(), LV_ALIGN_CENTER, 0, -10);

    // Message / error label
    message_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(message_label_, pal.error, 0);
    lv_label_set_text(message_label_, "");
    lv_obj_align(message_label_, LV_ALIGN_BOTTOM_MID, 0, -4);

    set_stage(Stage::EnterNew);
}

void SetupPinScreen::on_show()
{
    pin_entry_.reset();
    checking_ = false;
    set_stage(Stage::EnterNew);
}

bool SetupPinScreen::on_input(InputAction action)
{
    if (checking_) {
        return true; // swallow everything while the async set_pin() runs
    }

    if (stage_ == Stage::MismatchError) {
        if (action == InputAction::OkShort || action == InputAction::BackShort) {
            set_stage(Stage::EnterNew);
            pin_entry_.reset();
        }
        return true;
    }

    const bool consumed = pin_entry_.on_input(action);

    if (consumed && pin_entry_.is_complete()) {
        try_finish();
    }

    return consumed;
}

void SetupPinScreen::set_stage(Stage stage)
{
    stage_ = stage;
    lv_label_set_text(message_label_, "");

    switch (stage) {
        case Stage::EnterNew:
            lv_label_set_text(prompt_label_, "Enter new PIN");
            break;
        case Stage::Confirm:
            lv_label_set_text(prompt_label_, "Confirm PIN");
            break;
        case Stage::MismatchError:
            lv_label_set_text(prompt_label_, "");
            show_message("PINs do not match!");
            break;
    }
}

void SetupPinScreen::try_finish()
{
    if (stage_ == Stage::EnterNew) {
        // Save first entry and move to confirmation
        std::strncpy(first_pin_, pin_entry_.pin(), sizeof(first_pin_) - 1);
        first_pin_[sizeof(first_pin_) - 1] = '\0';
        pin_entry_.reset();
        set_stage(Stage::Confirm);
        return;
    }

    if (stage_ == Stage::Confirm) {
        const char* second_pin = pin_entry_.pin();

        if (std::strcmp(first_pin_, second_pin) != 0) {
            // Mismatch — show error and restart
            ESP_LOGW(TAG, "PIN confirmation mismatch");
            set_stage(Stage::MismatchError);
            return;
        }

        // Save PIN length BEFORE calling set_pin(): set_pin() validates
        // the new PIN's length against settings::all().security.pin_length
        // internally, so if that's still the old/default value (e.g. 6)
        // and the user chose a different length (e.g. 4), set_pin()
        // would reject it as invalid. Updating the setting first, then
        // calling set_pin(), keeps both consistent.
        settings::SecuritySettings updated = settings::all().security;
        updated.pin_length = static_cast<uint8_t>(std::strlen(first_pin_));

        if (!settings::set_security(updated)) {
            show_message("Failed to save PIN length setting");
            ESP_LOGE(TAG, "set_security() failed before set_pin()");
            return;
        }

        // Match — save PIN (async, see setup_pin_screen.hpp)
        checking_ = true;
        show_message("Checking...");
        async_check_.start_set_pin(first_pin_, nullptr, &SetupPinScreen::on_set_pin_done, this);

        // Clear sensitive data from RAM -- start_set_pin() already
        // copied it internally, safe to wipe our own copy now.
        std::memset(first_pin_, 0, sizeof(first_pin_));
    }
}

void SetupPinScreen::on_set_pin_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<SetupPinScreen*>(ctx)->handle_set_pin_result(result);
}

void SetupPinScreen::handle_set_pin_result(security::pin::VerifyResult result)
{
    checking_ = false;

    if (result != security::pin::VerifyResult::Success) {
        show_message("Failed to save PIN");
        ESP_LOGE(TAG, "set_pin() failed");
        return;
    }

    // Unlock without re-verifying: set_pin() just succeeded for this
    // exact PIN (proving old-PIN knowledge first, if one existed), so
    // a second full PBKDF2 pass to verify it again immediately after
    // would be redundant -- another ~10 seconds wasted for a foregone
    // conclusion. See security::lock::unlock_after_pin_set()'s doc
    // comment.
    if (!security::lock::unlock_after_pin_set()) {
        show_message("Unlock failed after PIN set");
        ESP_LOGE(TAG, "unlock_after_pin_set() failed after set_pin()");
        return;
    }

    ESP_LOGI(TAG, "PIN set and device unlocked");
    manager().replace(std::make_unique<MainMenu>());
}

void SetupPinScreen::show_message(const char* msg)
{
    lv_label_set_text(message_label_, msg);
}

} // namespace ui::screens
