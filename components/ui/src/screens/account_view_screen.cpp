#include "ui/screens/account_view_screen.hpp"

#include "ui/screens/account_edit_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/permission_manager.hpp"
#include "vault/vault.hpp"

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

const char* AccountViewScreen::title() const
{
    return "Account";
}

const char* AccountViewScreen::footer_hint() const
{
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

void AccountViewScreen::reload()
{
    // on_show() re-runs this every time the screen becomes active
    // again (e.g. returning from a future AccountEdit) -- clear the
    // previous tree first.
    lv_obj_clean(content_parent_);
    password_value_label_ = nullptr;
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
        lv_label_set_text_fmt(row, "%s: %s", label, value.c_str());
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 4, y);
        y += FIELD_SPACING;
    };

    // Per GUI.md 11: empty fields are not shown. Name/Category/
    // Favorite are skipped entirely -- vault::VaultEntry has none of
    // those fields, see account_view_screen.hpp.
    add_text_field("URL", entry_.url);
    add_text_field("Username", entry_.login);

    if (!entry_.password.empty()) {
        lv_obj_t* row = lv_label_create(parent);
        lv_obj_set_style_text_color(row, pal.primary_text, 0);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 4, y);
        password_value_label_ = row;
        update_password_label();
        y += FIELD_SPACING;
    }

    if (!entry_.totp_secret.empty()) {
        lv_obj_t* row = lv_label_create(parent);
        lv_obj_set_style_text_color(row, pal.secondary_text, 0);
        lv_label_set_text(row, "OTP: configured");
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
        case Action::PrintUrl:       return "Print URL";
        case Action::PrintUsername:  return "Print Username";
        case Action::PrintPassword:  return "Print Password";
        case Action::PrintOtp:       return "Print OTP";
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

        case Action::PrintUrl:
        case Action::PrintUsername:
        case Action::PrintPassword:
        case Action::PrintOtp: {
            // No PermissionManager operation exists for "Print URL"/
            // "Print Username" -- reusing PrintPassword's gate as a
            // placeholder, same decision already made in
            // QuickScreen. See account_view_screen.hpp.
            const security::permission::Operation op = (action == Action::PrintOtp)
                                                             ? security::permission::Operation::PrintOtp
                                                             : security::permission::Operation::PrintPassword;
            const security::permission::Result result = security::permission::check(op);

            if (result == security::permission::Result::Allowed) {
                // PLACEHOLDER: no USB HID OutputChannel implementation
                // yet (see interfaces::channels).
                ESP_LOGI(TAG, "%s: permission OK, USB HID not implemented yet", action_name(action));
                lv_label_set_text(status_label_, "USB typing: coming soon");
            } else {
                ESP_LOGI(TAG, "%s denied (%d)", action_name(action), static_cast<int>(result));
                lv_label_set_text(status_label_, "Not allowed");
            }
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

bool AccountViewScreen::on_input(InputAction action)
{
    if (!loaded_) {
        return false; // just the "not found" message; BACK pops normally
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
