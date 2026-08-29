#include "ui/screens/vault_list_screen.hpp"

#include "ui/screens/account_edit_screen.hpp"
#include "ui/screens/account_view_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "vault/vault.hpp"

#include "esp_log.h"

#include <memory>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.vault_list";

constexpr lv_coord_t FIRST_ITEM_Y = 4;
constexpr lv_coord_t ITEM_SPACING = 20;

} // namespace

const char* VaultListScreen::title() const
{
    return "Vault";
}

const char* VaultListScreen::footer_hint() const
{
    return "ROTATE  Select    OK  Open    Hold OK  New    BACK  Return";
}

void VaultListScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    const theme::Palette& pal = theme::current();

    empty_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(empty_label_, pal.secondary_text, 0);
    lv_label_set_text(empty_label_, "Vault is empty");
    lv_obj_center(empty_label_);
    lv_obj_add_flag(empty_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(content_parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    reload();
}

void VaultListScreen::on_show()
{
    lv_label_set_text(status_label_, "");
    reload();
}

void VaultListScreen::reload()
{
    // Drop any previously created row labels first -- on_show() calls
    // this every time the screen becomes active again, and the entry
    // count may have changed (once AccountEdit exists to actually
    // change it; harmless no-op re-render until then).
    for (size_t i = 0; i < MAX_ROWS; ++i) {
        if (row_labels_[i] != nullptr) {
            lv_obj_del(row_labels_[i]);
            row_labels_[i] = nullptr;
        }
    }

    const size_t total = vault::entry_count();
    const size_t to_load = (total > MAX_ROWS) ? MAX_ROWS : total;

    entries_.assign(to_load, vault::VaultEntry{});
    if (to_load > 0) {
        vault::list_entries(entries_.data(), to_load, 0);
    }

    if (total > MAX_ROWS) {
        ESP_LOGW(TAG, "Vault has %u entries, only showing the first %u (see README.md)",
                 static_cast<unsigned>(total), static_cast<unsigned>(MAX_ROWS));
    }

    selected_ = 0;

    const theme::Palette& pal = theme::current();
    for (size_t i = 0; i < entries_.size(); ++i) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(label, pal.primary_text, 0);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4,
                     FIRST_ITEM_Y + static_cast<lv_coord_t>(ITEM_SPACING * i));
        row_labels_[i] = label;
    }

    if (entries_.empty()) {
        lv_obj_clear_flag(empty_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(empty_label_, LV_OBJ_FLAG_HIDDEN);
    }

    render();
}

void VaultListScreen::render()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < entries_.size(); ++i) {
        const vault::VaultEntry& entry = entries_[i];
        const bool is_selected = (i == selected_);
        const bool has_otp = !entry.totp_secret.empty();
        const char* login = entry.login.empty() ? "(no login)" : entry.login.c_str();

        lv_obj_set_style_text_color(row_labels_[i], is_selected ? pal.accent : pal.primary_text, 0);
        lv_label_set_text_fmt(row_labels_[i], "%s%s%s",
                               is_selected ? "> " : "", login, has_otp ? "  [OTP]" : "");
    }

    if (!entries_.empty()) {
        lv_obj_scroll_to_view(row_labels_[selected_], LV_ANIM_ON);
    }
}

void VaultListScreen::move_selection(int32_t delta)
{
    if (entries_.empty()) {
        return;
    }

    int32_t index = static_cast<int32_t>(selected_) + delta;
    const int32_t count = static_cast<int32_t>(entries_.size());

    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }

    selected_ = static_cast<size_t>(index);
    render();
}

void VaultListScreen::activate()
{
    if (entries_.empty()) {
        return;
    }

    const vault::VaultEntry& entry = entries_[selected_];
    ESP_LOGI(TAG, "Selected entry id=%lu login='%s'",
             static_cast<unsigned long>(entry.id), entry.login.c_str());
    manager().push(std::make_unique<AccountViewScreen>(entry.id));
}

bool VaultListScreen::on_input(InputAction action)
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

        case InputAction::OkLong:
            // Create a new entry -- there's no dedicated "+ Add" row,
            // this is the only entry point into AccountEditScreen for
            // a fresh entry (editing an existing one is reached via
            // AccountViewScreen's Edit action instead).
            manager().push(std::make_unique<AccountEditScreen>(vault::INVALID_ID));
            return true;

        case InputAction::BackShort:
            return false; // pop back to MainMenu

        default:
            return false;
    }
}

} // namespace ui::screens
