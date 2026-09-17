#include "ui/screens/account_view_screen.hpp"

#include "display/fonts.hpp"
#include "ui/screens/account_edit_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/permission_manager.hpp"
#include "rtc_time/rtc_time.hpp"
#include "totp/totp.hpp"
#include "vault/vault.hpp"
#include "usb/usb_service.hpp"

#include "esp_log.h"

#include <memory>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.account_view";

constexpr lv_coord_t FIELD_Y_START = 4;
constexpr lv_coord_t FIELD_SPACING = 16;
constexpr lv_coord_t ACTION_SPACING = 20;

} // namespace

AccountViewScreen::AccountViewScreen(uint32_t entry_id) : entry_id_(entry_id) {}

AccountViewScreen::~AccountViewScreen()
{
    // Safety net -- on_hide() (called by the Screen lifecycle before
    // Destroy, see screen.hpp) already deletes this normally. Checked
    // for null either way, lv_timer_del() on a null pointer would be
    // undefined behavior.
    if (otp_refresh_timer_ != nullptr) {
        lv_timer_del(otp_refresh_timer_);
        otp_refresh_timer_ = nullptr;
    }
}

const char* AccountViewScreen::title() const
{
    return (mode_ == Mode::RecoveryCodesList) ? "Recovery Codes" : "Account";
}

const char* AccountViewScreen::footer_hint() const
{
    if (mode_ == Mode::RecoveryCodesList) {
        return "ROTATE  Scroll    BACK  Return";
    }
    return "ROTATE  Select    OK  Run    BACK  Return";
}

void AccountViewScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;
    reload();
}

void AccountViewScreen::on_show()
{
    delete_confirm_pending_ = false;
    reload();
}

void AccountViewScreen::on_hide()
{
    if (otp_refresh_timer_ != nullptr) {
        lv_timer_del(otp_refresh_timer_);
        otp_refresh_timer_ = nullptr;
    }
}

void AccountViewScreen::reload()
{
    // on_show() re-runs this every time the screen becomes active
    // again (e.g. returning from a future AccountEdit) -- clear the
    // previous tree first.
    mode_ = Mode::Main; // always land back on the main view, never mid-scroll in the recovery-codes list
    lv_obj_clean(content_parent_);
    password_value_label_ = nullptr;
    if (otp_refresh_timer_ != nullptr) {
        // Defensive -- on_hide() already deletes this normally (see
        // this screen's own comment there), but lv_obj_clean() above
        // just destroyed whatever otp_value_label_ pointed to without
        // touching the timer itself (LVGL timers aren't part of the
        // object tree) -- a stale timer here would update a dangling
        // label handle.
        lv_timer_del(otp_refresh_timer_);
        otp_refresh_timer_ = nullptr;
    }
    otp_value_label_ = nullptr;
    for (size_t i = 0; i < MAX_ACTIONS; ++i) {
        action_labels_[i] = nullptr;
    }
    action_count_ = 0;
    selected_action_ = 0;
    password_revealed_ = false;
    status_label_ = nullptr;

    loaded_ = vault::get_entry(entry_id_, entry_);
    if (!loaded_) {
        const theme::Palette& pal = theme::current();
        lv_obj_t* msg = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(msg, pal.error, 0);
        lv_label_set_text(msg, "Entry not found");
        lv_obj_center(msg);
        return;
    }

    const lv_coord_t fields_bottom = build_fields(content_parent_);
    build_actions(content_parent_, fields_bottom + 4);

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_actions();
}

lv_coord_t AccountViewScreen::build_fields(lv_obj_t* parent)
{
    const theme::Palette& pal = theme::current();
    lv_coord_t y = FIELD_Y_START;

    auto add_text_field = [&](const char* label, const std::string& value) {
        if (value.empty()) {
            return;
        }
        lv_obj_t* row = lv_label_create(parent);
        lv_obj_set_style_text_color(row, pal.primary_text, 0);
        // value is user-entered (URL/Username/Category/Notes), could
        // be Cyrillic -- see widgets::TextEntry's own comment for why
        // this is set per-label, not as a global default theme font.
        lv_obj_set_style_text_font(row, &keykeeper_cyrillic_16, 0);
        lv_label_set_text_fmt(row, "%s: %s", label, value.c_str());
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 4, y);
        y += FIELD_SPACING;
    };

    // Per GUI.md 11: empty fields are not shown. "Username" is
    // VaultEntry::login (no separate Name field exists).
    add_text_field("URL", entry_.url);
    add_text_field("Username", entry_.login);
    add_text_field("Category", entry_.category);

    if (!entry_.password.empty()) {
        lv_obj_t* row = lv_label_create(parent);
        lv_obj_set_style_text_color(row, pal.primary_text, 0);
        lv_obj_set_style_text_font(row, &keykeeper_cyrillic_16, 0); // password could contain Cyrillic now too
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 4, y);
        password_value_label_ = row;
        update_password_label();
        y += FIELD_SPACING;
    }

    if (!entry_.totp_secret.empty()) {
        lv_obj_t* row = lv_label_create(parent);
        lv_obj_set_style_text_color(row, pal.secondary_text, 0);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 4, y);
        otp_value_label_ = row;
        update_otp_label();

        // Refresh once a second -- cheap (one HMAC-SHA-1 call, unlike
        // the PBKDF2 checks elsewhere in this project that needed a
        // background worker task) so a plain LVGL timer on the UI
        // thread is fine. Deleted in on_hide() -- see that function's
        // own comment for why leaving it running while this screen
        // isn't visible would be wrong (dangling label handle after
        // the next reload(), wasted work while hidden).
        otp_refresh_timer_ = lv_timer_create(&AccountViewScreen::otp_refresh_timer_cb, 1000, this);

        y += FIELD_SPACING;
    }

    if (entry_.favorite) {
        lv_obj_t* row = lv_label_create(parent);
        lv_obj_set_style_text_color(row, pal.accent, 0);
        lv_label_set_text(row, "* Favorite");
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 4, y);
        y += FIELD_SPACING;
    }

    add_text_field("Notes", entry_.notes);

    return y;
}

void AccountViewScreen::update_password_label()
{
    if (password_value_label_ == nullptr) {
        return;
    }
    if (password_revealed_) {
        lv_label_set_text_fmt(password_value_label_, "Password: %s", entry_.password.c_str());
    } else {
        // Fixed-width mask -- deliberately not matching the real
        // length, so the mask itself doesn't leak that.
        lv_label_set_text(password_value_label_, "Password: ********");
    }
}

void AccountViewScreen::update_otp_label()
{
    if (otp_value_label_ == nullptr) {
        return;
    }

    char code[8];
    if (totp::generate(entry_.totp_secret, code, sizeof(code))) {
        lv_label_set_text_fmt(otp_value_label_, "OTP: %s (%us)", code,
                               static_cast<unsigned>(totp::seconds_remaining()));
    } else if (rtc_time::is_synced()) {
        // Time is fine, so the secret itself is the problem (invalid
        // Base32) -- shouldn't normally happen since AccountEditScreen
        // takes whatever was typed as-is, but stay clear about which
        // of the two failure reasons this is.
        lv_label_set_text(otp_value_label_, "OTP: invalid secret");
    } else {
        lv_label_set_text(otp_value_label_, "OTP: no time sync (connect WiFi)");
    }
}

void AccountViewScreen::otp_refresh_timer_cb(lv_timer_t* timer)
{
    auto* self = static_cast<AccountViewScreen*>(lv_timer_get_user_data(timer));
    self->update_otp_label();
}

void AccountViewScreen::build_actions(lv_obj_t* parent, lv_coord_t y_start)
{
    action_count_ = 0;
    auto add_action = [&](Action action) {
        if (action_count_ < MAX_ACTIONS) {
            available_actions_[action_count_++] = action;
        }
    };

    if (!entry_.password.empty()) {
        add_action(Action::RevealPassword);
    }
    add_action(Action::ToggleFavorite);
    if (!entry_.url.empty()) {
        add_action(Action::PrintUrl);
    }
    if (!entry_.login.empty()) {
        add_action(Action::PrintUsername);
    }
    if (!entry_.password.empty()) {
        add_action(Action::PrintPassword);
    }
    if (!entry_.totp_secret.empty()) {
        add_action(Action::PrintOtp);
    }
    if (!entry_.recovery_codes.empty()) {
        add_action(Action::ViewRecoveryCodes);
        add_action(Action::PrintRecoveryCodes);
    }
    add_action(Action::Edit);
    add_action(Action::Delete);

    for (size_t i = 0; i < action_count_; ++i) {
        lv_obj_t* label = lv_label_create(parent);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, y_start + static_cast<lv_coord_t>(ACTION_SPACING * i));
        action_labels_[i] = label;
    }
}

const char* AccountViewScreen::action_name(Action action) const
{
    switch (action) {
        case Action::RevealPassword: return "Reveal Password";
        case Action::ToggleFavorite: return entry_.favorite ? "Remove from Favorites" : "Add to Favorites";
        case Action::PrintUrl:       return "Print URL";
        case Action::PrintUsername:  return "Print Username";
        case Action::PrintPassword:  return "Print Password";
        case Action::PrintOtp:       return "Print OTP";
        case Action::ViewRecoveryCodes:  return "View Recovery Codes";
        case Action::PrintRecoveryCodes: return "Print Recovery Codes";
        case Action::Edit:           return "Edit";
        case Action::Delete:         return "Delete";
    }
    return "";
}

void AccountViewScreen::render_actions()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < action_count_; ++i) {
        const bool is_selected = (i == selected_action_);
        const bool is_delete_confirm =
            is_selected && available_actions_[i] == Action::Delete && delete_confirm_pending_;

        lv_obj_set_style_text_color(
            action_labels_[i],
            is_delete_confirm ? pal.error : (is_selected ? pal.accent : pal.primary_text), 0);

        const char* name = action_name(available_actions_[i]);
        if (is_delete_confirm) {
            lv_label_set_text_fmt(action_labels_[i], "> %s (confirm?)", name);
        } else {
            lv_label_set_text_fmt(action_labels_[i], "%s%s", is_selected ? "> " : "", name);
        }
    }

    if (action_count_ > 0) {
        lv_obj_scroll_to_view(action_labels_[selected_action_], LV_ANIM_ON);
    }
}

void AccountViewScreen::move_selection(int32_t delta)
{
    if (action_count_ == 0) {
        return;
    }

    delete_confirm_pending_ = false; // moving away cancels a pending delete confirm

    int32_t index = static_cast<int32_t>(selected_action_) + delta;
    const int32_t count = static_cast<int32_t>(action_count_);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_action_ = static_cast<size_t>(index);

    lv_label_set_text(status_label_, "");
    render_actions();
}

void AccountViewScreen::activate()
{
    if (action_count_ == 0) {
        return;
    }

    const Action action = available_actions_[selected_action_];
    if (action != Action::Delete) {
        delete_confirm_pending_ = false;
    }

    switch (action) {
        case Action::RevealPassword:
            password_revealed_ = !password_revealed_;
            update_password_label();
            lv_label_set_text(status_label_, "");
            return;

        case Action::ToggleFavorite: {
            vault::VaultEntry updated = entry_;
            updated.favorite = !updated.favorite;

            if (vault::update_entry(updated)) {
                entry_.favorite = updated.favorite;
                ESP_LOGI(TAG, "Entry %lu favorite -> %d", static_cast<unsigned long>(entry_id_),
                         entry_.favorite ? 1 : 0);
                // Rebuild: the Favorite field row and this action's
                // label both depend on entry_.favorite.
                reload();
            } else {
                ESP_LOGE(TAG, "Failed to toggle favorite for entry %lu",
                         static_cast<unsigned long>(entry_id_));
                lv_label_set_text(status_label_, "Update failed");
            }
            return;
        }

        case Action::PrintUrl:
        case Action::PrintUsername:
        case Action::PrintPassword:
        case Action::PrintOtp: {
            const security::permission::Operation op = (action == Action::PrintOtp)
                                                             ? security::permission::Operation::PrintOtp
                                                             : security::permission::Operation::PrintPassword;
            const security::permission::Result result = security::permission::check(op);

            if (result == security::permission::Result::Allowed) {
                switch (action) {
                    case Action::PrintUrl:
                        usb::print_field(entry_, usb::Field::Url);
                        break;
                    case Action::PrintUsername:
                        usb::print_field(entry_, usb::Field::Login);
                        break;
                    case Action::PrintPassword:
                        usb::print_field(entry_, usb::Field::Password);
                        break;
                    case Action::PrintOtp:
                        usb::print_field(entry_, usb::Field::Otp);
                        break;
                    default:
                        break;
                }
                lv_label_set_text(status_label_, usb::last_status());
            } else {
                ESP_LOGI(TAG, "%s denied (%d)", action_name(action), static_cast<int>(result));
                lv_label_set_text(status_label_, "Not allowed");
            }
            return;
        }

        case Action::ViewRecoveryCodes:
            enter_recovery_codes_list();
            return;

        case Action::PrintRecoveryCodes: {
            const security::permission::Result result =
                security::permission::check(security::permission::Operation::PrintPassword);
            if (result != security::permission::Result::Allowed) {
                ESP_LOGI(TAG, "%s denied (%d)", action_name(action), static_cast<int>(result));
                lv_label_set_text(status_label_, "Not allowed");
                return;
            }

            std::string text;
            for (const vault::RecoveryCode& rc : entry_.recovery_codes) {
                if (rc.used) {
                    continue;
                }
                if (!text.empty()) {
                    text += '\n';
                }
                text += rc.code;
            }

            if (text.empty()) {
                lv_label_set_text(status_label_, "No unused codes left");
                return;
            }

            usb::type_string(text);
            lv_label_set_text(status_label_, usb::last_status());
            return;
        }

        case Action::Edit:
            manager().push(std::make_unique<AccountEditScreen>(entry_id_));
            return;

        case Action::Delete:
            if (!delete_confirm_pending_) {
                delete_confirm_pending_ = true;
                render_actions();
                lv_label_set_text(status_label_, "Press OK again to delete");
                return;
            }

            if (vault::delete_entry(entry_id_)) {
                ESP_LOGI(TAG, "Entry %lu deleted", static_cast<unsigned long>(entry_id_));
                delete_confirm_pending_ = false;
                manager().pop(); // back to VaultListScreen, which reloads on_show()
            } else {
                ESP_LOGE(TAG, "Failed to delete entry %lu", static_cast<unsigned long>(entry_id_));
                delete_confirm_pending_ = false;
                lv_label_set_text(status_label_, "Delete failed");
                render_actions();
            }
            return;
    }
}

void AccountViewScreen::enter_recovery_codes_list()
{
    mode_ = Mode::RecoveryCodesList;
    selected_recovery_code_ = 0;
    build_recovery_codes_list();
}

void AccountViewScreen::build_recovery_codes_list()
{
    lv_obj_clean(content_parent_);

    const theme::Palette& pal = theme::current();
    constexpr lv_coord_t ROW_Y_START = 4;
    constexpr lv_coord_t ROW_SPACING = 20;

    for (size_t i = 0; i < entry_.recovery_codes.size(); ++i) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(label, pal.primary_text, 0);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        recovery_code_labels_[i] = label;
    }

    render_recovery_codes_list();
}

void AccountViewScreen::render_recovery_codes_list()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < entry_.recovery_codes.size(); ++i) {
        const vault::RecoveryCode& rc = entry_.recovery_codes[i];
        const bool is_selected = (i == selected_recovery_code_);

        // No strikethrough here -- kept to plain text + a bracketed
        // marker, since this list is read-only on-device anyway (see
        // this screen's own header comment on why marking one used is
        // web-UI-only) and a marker reads fine even on this display's
        // smaller font.
        const lv_color_t color = is_selected ? pal.accent : (rc.used ? pal.secondary_text : pal.primary_text);
        lv_obj_set_style_text_color(recovery_code_labels_[i], color, 0);
        lv_label_set_text_fmt(recovery_code_labels_[i], "%s%s%s", is_selected ? "> " : "", rc.code.c_str(),
                               rc.used ? " [used]" : "");
    }

    if (!entry_.recovery_codes.empty()) {
        lv_obj_scroll_to_view(recovery_code_labels_[selected_recovery_code_], LV_ANIM_ON);
    }
}

void AccountViewScreen::move_recovery_code_selection(int32_t delta)
{
    const size_t count = entry_.recovery_codes.size();
    if (count == 0) {
        return;
    }

    int32_t index = static_cast<int32_t>(selected_recovery_code_) + delta;
    const int32_t total = static_cast<int32_t>(count);
    if (index < 0) {
        index = total - 1;
    }
    if (index >= total) {
        index = 0;
    }
    selected_recovery_code_ = static_cast<size_t>(index);

    render_recovery_codes_list();
}

bool AccountViewScreen::on_input(InputAction action)
{
    if (!loaded_) {
        return false; // just the "not found" message; BACK pops normally
    }

    if (mode_ == Mode::RecoveryCodesList) {
        switch (action) {
            case InputAction::RotateLeft:
                move_recovery_code_selection(-1);
                return true;

            case InputAction::RotateRight:
                move_recovery_code_selection(+1);
                return true;

            case InputAction::BackShort:
                mode_ = Mode::Main;
                reload();
                return true;

            default:
                return true; // swallow OK/etc -- nothing to activate in a read-only list
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
            return false; // pop back to VaultListScreen

        default:
            return false;
    }
}

} // namespace ui::screens
