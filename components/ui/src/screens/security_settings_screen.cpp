#include "ui/screens/security_settings_screen.hpp"

#include "ui/localization.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/pin_manager.hpp"
#include "settings/settings.hpp"
#include "vault/vault_repository.hpp"
#include "wifi/wifi_service.hpp"

#include "esp_log.h"
#include "esp_system.h"

#include <cstdio>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.security_settings";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

// Auto Lock's allowed timeout values -- off, then 1/3/5 min, then
// 5-minute steps up to 100 min. Not a simple linear range (1/3/5 min
// are irregular before the steady 5-min cadence kicks in), so this is
// a lookup table cycled by index, not arithmetic on raw seconds.
constexpr uint32_t AUTO_LOCK_VALUES_S[] = {
    0, // off
    60, 180, 300, // 1, 3, 5 min
    600, 900, 1200, 1500, 1800, 2100, 2400, 2700, 3000, // 10..50 min, step 5
    3300, 3600, 3900, 4200, 4500, 4800, 5100, 5400, 5700, 6000, // 55..100 min, step 5
};
constexpr size_t AUTO_LOCK_VALUES_COUNT = sizeof(AUTO_LOCK_VALUES_S) / sizeof(AUTO_LOCK_VALUES_S[0]);

size_t find_closest_auto_lock_index(uint32_t seconds)
{
    size_t best = 0;
    uint32_t best_diff = 0xFFFFFFFFu;
    for (size_t i = 0; i < AUTO_LOCK_VALUES_COUNT; ++i) {
        const uint32_t diff = (AUTO_LOCK_VALUES_S[i] > seconds) ? (AUTO_LOCK_VALUES_S[i] - seconds)
                                                                  : (seconds - AUTO_LOCK_VALUES_S[i]);
        if (diff < best_diff) {
            best_diff = diff;
            best = i;
        }
    }
    return best;
}

} // namespace

const char* SecuritySettingsScreen::title() const
{
    return i18n::tr(i18n::Key::SecurityTitle);
}

const char* SecuritySettingsScreen::footer_hint() const
{
    if (checking_) {
        return i18n::tr(i18n::Key::Checking);
    }
    switch (mode_) {
        case Mode::Adjust:
            return i18n::tr(i18n::Key::AdjustFooter);
        case Mode::ChangingPin:
            return i18n::tr(i18n::Key::ChangingPinFooter);
        case Mode::SettingDuressPin:
            return i18n::tr(i18n::Key::ChangingPinFooter);
        default:
            return i18n::tr(i18n::Key::OkOpenBackCancel);
    }
}

void SecuritySettingsScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    const settings::SecuritySettings& s = settings::all().security;
    ///auto_lock_enabled_ = s.auto_lock_enabled;
    auto_lock_timeout_s_ = s.auto_lock_enabled ? s.auto_lock_timeout_s : 0;
    web_ui_view_accounts_ = (s.web_ui_permissions & settings::WEB_UI_VIEW_ACCOUNTS) != 0;
    pin_entry_dial_mode_ = s.pin_entry_dial_mode;
    dial_last_digit_reverses_ = s.dial_last_digit_reverses;

    build_rows();
}

void SecuritySettingsScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
}

void SecuritySettingsScreen::build_rows()
{
    for (size_t i = 0; i < ROW_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        row_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_rows();
}

void SecuritySettingsScreen::render_rows()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool is_selected = (i == selected_row_);
        const bool is_adjusting = is_selected && mode_ == Mode::Adjust;
        lv_obj_set_style_text_color(
            row_labels_[i], is_adjusting ? pal.warning : (is_selected ? pal.accent : pal.primary_text), 0);

        const char* prefix = is_selected ? "> " : "";

        switch (static_cast<Row>(i)) {
            case Row::ChangePin:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::ChangePinRowFmt), prefix);
                break;

            case Row::DuressPin:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::DuressPinRowFmt), prefix,
                                       security::pin::has_duress_pin() ? i18n::tr(i18n::Key::Configured)
                                                                        : i18n::tr(i18n::Key::NotSetValue));
                break;

            case Row::FactoryReset:
                if (is_selected && factory_reset_confirm_pending_) {
                    lv_obj_set_style_text_color(row_labels_[i], pal.error, 0);
                    lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::FactoryResetConfirmRowFmt), prefix);
                } else {
                    lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::FactoryResetRowFmt), prefix);
                }
                break;

            case Row::AutoLock:
                if (auto_lock_timeout_s_ == 0) {
                    lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::AutoLockOffRowFmt), prefix);
                } else {
                    lv_label_set_text_fmt(
                        row_labels_[i],
                        i18n::tr(i18n::Key::AutoLockMinRowFmt),
                        prefix,
                        static_cast<unsigned long>(auto_lock_timeout_s_ / 60));
                }
                break;

            case Row::WebUiViewAccounts:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::WebUiViewRowFmt), prefix,
                                    web_ui_view_accounts_ ? i18n::tr(i18n::Key::AllowedValue) : i18n::tr(i18n::Key::OffValue));
                break;

            case Row::PinEntryStyle:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::PinEntryRowFmt), prefix,
                                    pin_entry_dial_mode_ ? i18n::tr(i18n::Key::DialValue) : i18n::tr(i18n::Key::StandardValue));
                break;

            case Row::DialLastDigit:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::DialLastDigitRowFmt), prefix,
                                    dial_last_digit_reverses_ ? i18n::tr(i18n::Key::ReverseValue) : i18n::tr(i18n::Key::OkShortValue));
                break;

            case Row::Save:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::SaveRowFmt), prefix);
                
                break;
        }
    }

    // 6 rows sit right at the edge of what fits in the visible
    // content area -- keep the selected one scrolled into view (see
    // WifiSettingsScreen's identical fix; that screen hit this first
    // at 7 rows, but the same gap existed here too).
    if (row_labels_[selected_row_] != nullptr) {
        lv_obj_scroll_to_view(row_labels_[selected_row_], LV_ANIM_ON);
    }
}

void SecuritySettingsScreen::move_selection(int32_t delta)
{
    int32_t index = static_cast<int32_t>(selected_row_) + delta;
    const int32_t count = static_cast<int32_t>(ROW_COUNT);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_row_ = static_cast<size_t>(index);

    factory_reset_confirm_pending_ = false;
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render_rows();
}

void SecuritySettingsScreen::adjust_value(int32_t delta)
{
    switch (static_cast<Row>(selected_row_)) {
        
        case Row::AutoLock: {
            const size_t idx = find_closest_auto_lock_index(auto_lock_timeout_s_);
            int32_t new_idx = static_cast<int32_t>(idx) + delta;
            if (new_idx < 0) {
                new_idx = 0;
            }
            if (new_idx >= static_cast<int32_t>(AUTO_LOCK_VALUES_COUNT)) {
                new_idx = static_cast<int32_t>(AUTO_LOCK_VALUES_COUNT) - 1;
            }
            auto_lock_timeout_s_ = AUTO_LOCK_VALUES_S[new_idx];
            break;
        }

        case Row::WebUiViewAccounts:
            web_ui_view_accounts_ = (delta > 0);
            break;

        case Row::PinEntryStyle:
            // DIAGNOSTIC -- logged before AND after the toggle so a
            // captured log shows directly whether this handler is
            // even being reached, how many times per physical click,
            // and what value results each time -- a confirmed
            // real-hardware report says this row (and DialLastDigit)
            // don't visibly change on rotation while a same-pattern
            // row elsewhere (WebUiViewAccounts, unmodified at the
            // time) did, which rules out a simple "double-dispatch
            // cancels an XOR toggle" explanation for THIS specific
            // case -- switched from `!bool` to setting from delta's
            // sign anyway (harmless and more robust either way), but
            // the real cause is still unconfirmed pending this log.
            ESP_LOGI(TAG, "PinEntryStyle adjust: delta=%d before=%d", static_cast<int>(delta),
                     static_cast<int>(pin_entry_dial_mode_));
            pin_entry_dial_mode_ = (delta > 0);
            ESP_LOGI(TAG, "PinEntryStyle adjust: after=%d", static_cast<int>(pin_entry_dial_mode_));
            break;

        case Row::DialLastDigit:
            ESP_LOGI(TAG, "DialLastDigit adjust: delta=%d before=%d", static_cast<int>(delta),
                     static_cast<int>(dial_last_digit_reverses_));
            dial_last_digit_reverses_ = (delta > 0);
            ESP_LOGI(TAG, "DialLastDigit adjust: after=%d", static_cast<int>(dial_last_digit_reverses_));
            break;

        default:
            break;
    }

    render_rows();
}

void SecuritySettingsScreen::activate()
{
    const auto row = static_cast<Row>(selected_row_);

    if (row == Row::ChangePin) {
        begin_change_pin();
        return;
    }
    if (row == Row::DuressPin) {
        begin_duress_pin_setup();
        return;
    }
    if (row == Row::FactoryReset) {
        if (!factory_reset_confirm_pending_) {
            factory_reset_confirm_pending_ = true;
            render_rows();
            lv_label_set_text(status_label_, i18n::tr(i18n::Key::ConfirmEraseEverything));
            return;
        }
        perform_factory_reset();
        return;
    }
    if (row == Row::Save) {
        save();
        return;
    }

    mode_ = Mode::Adjust;
    render_rows();
}

void SecuritySettingsScreen::save()
{
    settings::SecuritySettings updated = settings::all().security;
    updated.auto_lock_enabled   = auto_lock_timeout_s_ > 0;
    updated.auto_lock_timeout_s = auto_lock_timeout_s_;
    updated.web_ui_permissions =
        web_ui_view_accounts_ ? settings::WEB_UI_VIEW_ACCOUNTS : settings::WEB_UI_NONE;
    updated.pin_entry_dial_mode = pin_entry_dial_mode_;
    updated.dial_last_digit_reverses = dial_last_digit_reverses_;

    if (settings::set_security(updated)) {
        ESP_LOGI(TAG, "Security settings saved");
        manager().pop();
    } else {
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::SaveFailed));
    }
}

void SecuritySettingsScreen::begin_change_pin()
{
    mode_ = Mode::ChangingPin;
    change_step_ = security::pin::has_pin() ? ChangePinStep::Old : ChangePinStep::New;
    old_pin_.clear();
    new_pin_.clear();
    show_pin_step();
}

void SecuritySettingsScreen::show_pin_step(const char* error /* = nullptr */)
{
    lv_obj_clean(content_parent_);
    status_label_ = nullptr;        // ← объект уничтожен, честно обнуляем

    const theme::Palette& pal = theme::current();
    lv_obj_t* header = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(header, pal.secondary_text, 0);

    const char* text = "";
    switch (change_step_) {
        case ChangePinStep::Old:     text = i18n::tr(i18n::Key::EnterCurrentPin); break;
        case ChangePinStep::New:     text = i18n::tr(i18n::Key::EnterNewPin); break;
        case ChangePinStep::Confirm: text = i18n::tr(i18n::Key::ConfirmNewPin); break;
    }
    lv_label_set_text(header, text);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 4);

    if (error != nullptr) {                // ← ошибка показывается внутри шага
        lv_obj_t* err = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(err, pal.warning, 0);
        lv_label_set_text(err, error);
        lv_obj_align(err, LV_ALIGN_TOP_MID, 0, 24);
    }

    widgets::PinEntry::Config cfg{};

    if (change_step_ == ChangePinStep::Old) {
        // Box count = the ACTUAL stored PIN's length
        // (security::pin::stored_pin_length()), not
        // settings::all().security.pin_length -- see that function's
        // own doc comment; same reasoning as LockScreen.
        cfg.length = security::pin::stored_pin_length();
        if (cfg.length < 4 || cfg.length > 6) {
            cfg.length = 6;
        }
        cfg.min_length = cfg.length;
        cfg.finish_on_short = true;
    } else {
        cfg.length = 6;
        cfg.min_length = 4;
        cfg.finish_on_short = false;
    }

    pin_entry_.init(content_parent_, cfg);
}

void SecuritySettingsScreen::handle_pin_step_complete()
{
    switch (change_step_) {
        case ChangePinStep::Old: {
            old_pin_ = pin_entry_.pin();
            pin_entry_.reset();

            checking_ = true;
            // status_label_ is nullptr while in the PIN-step UI (see
            // show_pin_step()) -- reuse its own message-display path
            // instead of touching a null label directly.
            show_pin_step(i18n::tr(i18n::Key::Checking));
            async_check_.start(AsyncPinCheck::Kind::Verify, old_pin_.c_str(),
                                &SecuritySettingsScreen::on_old_pin_check_done, this);
            return;
        }

        case ChangePinStep::New:
            new_pin_ = pin_entry_.pin();
            pin_entry_.reset();
            change_step_ = ChangePinStep::Confirm;
            show_pin_step();
            return;

        case ChangePinStep::Confirm: {
            const std::string confirm_pin = pin_entry_.pin();
            pin_entry_.reset();

            if (confirm_pin != new_pin_) {
                old_pin_.clear();
                new_pin_.clear();
                mode_ = Mode::Browse;
                lv_obj_clean(content_parent_);
                build_rows();
                ESP_LOGI(TAG, "PIN change cancelled -- confirmation did not match");
                lv_label_set_text(status_label_, i18n::tr(i18n::Key::PinsDidNotMatch));
                return;
            }

            // Update the pin_length setting BEFORE calling set_pin():
            // set_pin() validates the new PIN's length against
            // settings::all().security.pin_length internally, so it
            // must already reflect the new length or a legitimately
            // shorter/longer new PIN would be rejected. Rolled back in
            // handle_set_pin_result() if set_pin() fails, so a failed
            // change never leaves pin_length out of sync with the
            // still-active (old) PIN.
            previous_pin_length_ = settings::all().security.pin_length;

            settings::SecuritySettings updated = settings::all().security;
            updated.pin_length = static_cast<uint8_t>(new_pin_.length());

            if (!settings::set_security(updated)) {
                ESP_LOGE(TAG, "Failed to save PIN length setting before set_pin()");
                old_pin_.clear();
                new_pin_.clear();
                mode_ = Mode::Browse;
                lv_obj_clean(content_parent_);
                build_rows();
                lv_label_set_text(status_label_, i18n::tr(i18n::Key::PinChangeFailed));
                return;
            }

            checking_ = true;
            show_pin_step(i18n::tr(i18n::Key::Checking));
            if (!old_pin_.empty()) {
                // Normal case -- Old-PIN step already verified it
                // (async), so set_pin() re-verifying it a second time
                // right here would just be a redundant ~10s PBKDF2
                // pass. See set_pin_after_verify()'s own doc comment.
                async_check_.start_set_pin_after_verify(new_pin_.c_str(), &SecuritySettingsScreen::on_set_pin_done,
                                                          this);
            } else {
                // Defensive fallback for change_step_ starting at New
                // (no old PIN to verify at all) -- shouldn't happen in
                // practice, since this screen requires has_pin() to be
                // reachable at all, but keeps the old, safe behavior
                // if it somehow does.
                async_check_.start_set_pin(new_pin_.c_str(), nullptr, &SecuritySettingsScreen::on_set_pin_done,
                                            this);
            }
            return;
        }
    }
}

void SecuritySettingsScreen::on_set_pin_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<SecuritySettingsScreen*>(ctx)->handle_set_pin_result(result);
}

void SecuritySettingsScreen::handle_set_pin_result(security::pin::VerifyResult result)
{
    checking_ = false;

    const bool ok = (result == security::pin::VerifyResult::Success);
    if (!ok) {
        settings::SecuritySettings rollback = settings::all().security;
        rollback.pin_length = previous_pin_length_;
        settings::set_security(rollback);
    }

    old_pin_.clear();
    new_pin_.clear();
    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows();

    if (ok) {
        ESP_LOGI(TAG, "PIN changed");
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::PinChanged));
    } else {
        ESP_LOGI(TAG, "PIN change failed");
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::PinChangeFailed));
    }
}

void SecuritySettingsScreen::on_old_pin_check_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<SecuritySettingsScreen*>(ctx)->handle_old_pin_result(result);
}

void SecuritySettingsScreen::handle_old_pin_result(security::pin::VerifyResult result)
{
    checking_ = false;

    if (result != security::pin::VerifyResult::Success) {
        old_pin_.clear();

        if (result == security::pin::VerifyResult::LockedOut) {
            show_pin_step("Locked out, try later");
        } else if (result == security::pin::VerifyResult::WipeRequired) {
            // NOTE: reaching the wipe threshold here (verifying the OLD
            // PIN while changing it) does NOT trigger an actual wipe --
            // only LockScreen's unlock path does. The device is already
            // Unlocked to reach this screen at all, so the vault isn't
            // protected by a wipe here either way; this is a deliberate
            // scope decision, not an oversight -- flagged for you to
            // confirm it's the behavior you want.
            show_pin_step(i18n::tr(i18n::Key::TooManyFailedAttempts));
        } else {
            const uint8_t remaining = security::pin::attempts_remaining();
            char buf[48];
            if (remaining > 0) {
                std::snprintf(buf, sizeof(buf), "Wrong current PIN, %u left",
                              static_cast<unsigned>(remaining));
            } else {
                const uint8_t until_wipe = security::pin::attempts_until_wipe();
                std::snprintf(buf, sizeof(buf), "Wrong PIN! %u attempts until vault wipe",
                              static_cast<unsigned>(until_wipe));
            }
            show_pin_step(buf);
        }
        return;
    }

    change_step_ = ChangePinStep::New;
    show_pin_step();
}

void SecuritySettingsScreen::cancel_change_pin()
{
    old_pin_.clear();
    new_pin_.clear();
    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows();
    lv_label_set_text(status_label_, i18n::tr(i18n::Key::PinChangeCancelled));
}

void SecuritySettingsScreen::begin_duress_pin_setup()
{
    mode_ = Mode::SettingDuressPin;
    duress_step_ = DuressPinStep::CurrentPin;
    duress_current_pin_.clear();
    duress_new_pin_.clear();
    show_duress_pin_step();
}

void SecuritySettingsScreen::show_duress_pin_step(const char* error /* = nullptr */)
{
    lv_obj_clean(content_parent_);
    status_label_ = nullptr; // object just got destroyed above -- honestly null it out

    const theme::Palette& pal = theme::current();
    lv_obj_t* header = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(header, pal.secondary_text, 0);

    const char* text = "";
    switch (duress_step_) {
        case DuressPinStep::CurrentPin: text = i18n::tr(i18n::Key::EnterCurrentPin); break;
        case DuressPinStep::EnterNew:   text = i18n::tr(i18n::Key::EnterDuressPin); break;
        case DuressPinStep::Confirm:    text = i18n::tr(i18n::Key::ConfirmDuressPin); break;
    }
    lv_label_set_text(header, text);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 4);

    if (error != nullptr) {
        lv_obj_t* err = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(err, pal.warning, 0);
        lv_label_set_text(err, error);
        lv_obj_align(err, LV_ALIGN_TOP_MID, 0, 24);
    }

    // Deliberately NOT fixed at settings::all().security.pin_length
    // for the CurrentPin step -- that setting can drift from the
    // actual stored PIN's real length, and PinEntry hard-caps entry
    // at cfg.length, so a too-short value there would make it
    // impossible to even type a longer real PIN. For EnterNew/Confirm,
    // by then duress_current_pin_ has already been verified
    // successfully, so ITS length is the real, true regular-PIN
    // length -- an exact box count there is both correct UX and safe,
    // since it's derived from a just-verified fact, not a setting
    // that could disagree with reality.
    widgets::PinEntry::Config cfg{};

    if (duress_step_ == DuressPinStep::CurrentPin) {
        // Box count = the ACTUAL stored PIN's length
        // (security::pin::stored_pin_length()), same as
        // LockScreen/Change PIN's Old step.
        cfg.length = security::pin::stored_pin_length();
        if (cfg.length < 4 || cfg.length > 6) {
            cfg.length = 6;
        }
        cfg.min_length = cfg.length;
        cfg.finish_on_short = true;
    } else {
        uint8_t len = static_cast<uint8_t>(duress_current_pin_.length());
        if (len < 4 || len > 6) {
            len = 6; // shouldn't happen (already verified), just a safe fallback
        }
        cfg.length = len;
        cfg.min_length = len;
        cfg.finish_on_short = true;
    }

    pin_entry_.init(content_parent_, cfg);
}

void SecuritySettingsScreen::handle_duress_pin_step_complete()
{
    switch (duress_step_) {
        case DuressPinStep::CurrentPin:
            duress_current_pin_ = pin_entry_.pin();
            pin_entry_.reset();

            // Verify the current regular PIN immediately after it is entered.
            // This keeps the first step honest: an incorrect current PIN
            // cannot advance to Duress PIN configuration. The check is
            // asynchronous so the UI remains responsive during PBKDF2.
            checking_ = true;
            show_duress_pin_step(i18n::tr(i18n::Key::Checking));
            async_check_.start(AsyncPinCheck::Kind::Verify, duress_current_pin_.c_str(),
                               &SecuritySettingsScreen::on_duress_current_pin_check_done, this);
            return;

        case DuressPinStep::EnterNew:
            duress_new_pin_ = pin_entry_.pin();
            pin_entry_.reset();
            duress_step_ = DuressPinStep::Confirm;
            show_duress_pin_step();
            return;

        case DuressPinStep::Confirm: {
            const std::string confirm_pin = pin_entry_.pin();
            pin_entry_.reset();

            if (confirm_pin != duress_new_pin_) {
                duress_current_pin_.clear();
                duress_new_pin_.clear();
                mode_ = Mode::Browse;
                lv_obj_clean(content_parent_);
                build_rows();
                lv_label_set_text(status_label_, i18n::tr(i18n::Key::DuressPinsDidNotMatch));
                return;
            }

            checking_ = true;
            show_duress_pin_step(i18n::tr(i18n::Key::Checking));
            // CurrentPin step already verified duress_current_pin_
            // (async) -- set_duress_pin() re-verifying it again here
            // would be a redundant ~10s PBKDF2 pass for nothing
            // (the duress hash itself is fast SHA-256, not PBKDF2).
            // See set_duress_pin_after_verify()'s own doc comment.
            async_check_.start_set_duress_pin_after_verify(duress_new_pin_.c_str(), duress_current_pin_.c_str(),
                                                             &SecuritySettingsScreen::on_duress_set_done, this);
            return;
        }
    }
}

void SecuritySettingsScreen::on_duress_current_pin_check_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<SecuritySettingsScreen*>(ctx)->handle_duress_current_pin_result(result);
}

void SecuritySettingsScreen::handle_duress_current_pin_result(security::pin::VerifyResult result)
{
    checking_ = false;

    if (result != security::pin::VerifyResult::Success) {
        duress_current_pin_.clear();

        if (result == security::pin::VerifyResult::LockedOut) {
            show_duress_pin_step("Locked out, try later");
        } else if (result == security::pin::VerifyResult::WipeRequired) {
            show_duress_pin_step(i18n::tr(i18n::Key::TooManyFailedAttempts));
        } else {
            const uint8_t remaining = security::pin::attempts_remaining();
            char buf[48];
            if (remaining > 0) {
                std::snprintf(buf, sizeof(buf), "Wrong current PIN, %u left",
                              static_cast<unsigned>(remaining));
            } else {
                const uint8_t until_wipe = security::pin::attempts_until_wipe();
                std::snprintf(buf, sizeof(buf), "Wrong PIN! %u attempts until vault wipe",
                              static_cast<unsigned>(until_wipe));
            }
            show_duress_pin_step(buf);
        }
        return;
    }

    duress_step_ = DuressPinStep::EnterNew;
    show_duress_pin_step();
}

void SecuritySettingsScreen::on_duress_set_done(security::pin::VerifyResult result, void* ctx)
{
    static_cast<SecuritySettingsScreen*>(ctx)->handle_duress_set_result(result);
}

void SecuritySettingsScreen::handle_duress_set_result(security::pin::VerifyResult result)
{
    checking_ = false;

    const bool ok = (result == security::pin::VerifyResult::Success);

    duress_current_pin_.clear();
    duress_new_pin_.clear();
    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows();

    if (ok) {
        ESP_LOGI(TAG, "Duress PIN configured");
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::DuressPinSet));
    } else {
        // Covers both "current PIN was wrong" and "duress PIN equals
        // the regular PIN" (set_duress_pin() rejects both, see
        // pin_manager.cpp) -- deliberately one generic message rather
        // than distinguishing them, so a wrong-current-PIN attempt
        // here doesn't leak useful timing/feedback beyond "it failed".
        ESP_LOGW(TAG, "Duress PIN setup failed");
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::DuressPinSetupFailed));
    }
}

void SecuritySettingsScreen::cancel_duress_pin_setup()
{
    duress_current_pin_.clear();
    duress_new_pin_.clear();
    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows();
    lv_label_set_text(status_label_, i18n::tr(i18n::Key::DuressPinSetupCancelled));
}

void SecuritySettingsScreen::perform_factory_reset()
{
    ESP_LOGW(TAG, "Factory reset requested from Security Settings");

    // Same order as the automatic brute-force wipe (LockScreen's
    // WipeRequired case): vault first, then the PIN, only proceeding
    // if the previous step succeeded.
    const bool vault_wiped = vault::repository::wipe();
    const bool pin_wiped = vault_wiped && security::pin::wipe();
    const bool settings_reset = pin_wiped && settings::reset_to_defaults();

    if (!settings_reset) {
        factory_reset_confirm_pending_ = false;
        ESP_LOGE(TAG, "Factory reset failed (vault=%d, pin=%d, settings=%d)", vault_wiped ? 1 : 0,
                  pin_wiped ? 1 : 0, settings_reset ? 1 : 0);
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::FactoryResetFailed));
        render_rows();
        return;
    }

    // settings::reset_to_defaults() only rewrites the STORED WiFi
    // config back to Disabled -- it doesn't stop an already-running
    // radio. apply_settings() re-reads that stored config and acts
    // on it immediately, matching WifiSettingsScreen's own Save.
    wifi::apply_settings();

    ESP_LOGW(TAG, "Factory reset complete -- restarting");
    lv_label_set_text(status_label_, i18n::tr(i18n::Key::ResetCompleteRestarting));
    esp_restart(); // does not return -- same pattern as BackupScreen's Restore, no delay needed
}

bool SecuritySettingsScreen::on_input(InputAction action)
{
    if (checking_) {
        return true; // swallow everything while the async check runs
    }

    if (mode_ == Mode::ChangingPin) {
        const bool consumed = pin_entry_.on_input(action);

        if (pin_entry_.is_complete()) {
            handle_pin_step_complete();
            return true;
        }
        if (!consumed) {
            // BackShort with nothing typed at this step -- cancel the
            // whole change-PIN flow, not just this one step.
            cancel_change_pin();
            return true;
        }
        return true;
    }

    if (mode_ == Mode::SettingDuressPin) {
        const bool consumed = pin_entry_.on_input(action);

        if (pin_entry_.is_complete()) {
            handle_duress_pin_step_complete();
            return true;
        }
        if (!consumed) {
            cancel_duress_pin_setup();
            return true;
        }
        return true;
    }

    if (mode_ == Mode::Adjust) {
        switch (action) {
            case InputAction::RotateLeft:
                adjust_value(-1);
                return true;
            case InputAction::RotateRight:
                adjust_value(+1);
                return true;
            case InputAction::OkShort:
            case InputAction::BackShort:
                mode_ = Mode::Browse;
                render_rows();
                return true;
            default:
                return false;
        }
    }

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
            return false; // pop, discarding unsaved auto-lock/permission changes (any PIN change already applied)

        default:
            return false;
    }
}

} // namespace ui::screens
