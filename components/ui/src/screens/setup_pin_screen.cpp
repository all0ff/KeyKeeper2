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
    switch (stage_) {
        case Stage::EnterNew:
            return "ROTATE  Digit    OK  Next    BACK  Erase";
        case Stage::Confirm:
            return "ROTATE  Digit    OK  Next    BACK  Erase";
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
    cfg.length = settings::all().security.pin_length;
    cfg.mask_confirmed = true;
    pin_entry_.init(content_parent, cfg);
    lv_obj_update_layout(content_parent); //заставляет LVGL пересчитать область экрана, в которой находится PinEntry.
    lv_obj_update_layout(pin_entry_.root()); //заставляет LVGL непосредственно рассчитать размер LV_SIZE_CONTENT + flex-контейнера PIN.
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
    set_stage(Stage::EnterNew);
}

bool SetupPinScreen::on_input(InputAction action)
{
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

        // Match — save PIN
        if (!security::pin::set_pin(first_pin_, nullptr)) {
            show_message("Failed to save PIN");
            ESP_LOGE(TAG, "set_pin() failed");
            return;
        }

        // Unlock (must happen before clearing first_pin_)
        const security::pin::VerifyResult unlock_result = security::lock::unlock(first_pin_);
        if (unlock_result != security::pin::VerifyResult::Success) {
            show_message("Unlock failed after PIN set");
            ESP_LOGE(TAG, "unlock() failed after set_pin()");
            return;
        }

        ESP_LOGI(TAG, "PIN set and device unlocked");

        // Clear sensitive data from RAM
        std::memset(first_pin_, 0, sizeof(first_pin_));

        manager().replace(std::make_unique<MainMenu>());
    }
}

void SetupPinScreen::show_message(const char* msg)
{
    lv_label_set_text(message_label_, msg);
}

} // namespace ui::screens
