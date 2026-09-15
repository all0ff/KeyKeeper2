#include "ui/screens/backup_screen.hpp"

#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "security/permission_manager.hpp"
#include "storage/sdcard.hpp"
#include "storage/storage.hpp"

#include "esp_log.h"
#include "esp_system.h"

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.backup";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

constexpr const char* ACTION_NAMES[] = {
    "Create Backup",
    "Restore Backup",
    "Export Vault",
    "Import Vault",
    "Refresh SD Card",
    "Format SD Card",
};

} // namespace

const char* BackupScreen::title() const
{
    if (mode_ == Mode::BackupList) return "Restore Backup";
    if (mode_ == Mode::ImportList) return "Import Vault";
    return "Backup";
}

const char* BackupScreen::footer_hint() const
{
    if (mode_ == Mode::BackupList) {
        return "OK  Select/Confirm    BACK  Return";
    }
    if (mode_ == Mode::ImportList) {
        return "OK  Import    BACK  Return";
    }
    return "OK  Run    BACK  Return";
}

void BackupScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;
    build_action_list();
}

void BackupScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
}

void BackupScreen::build_action_list()
{
    mode_ = Mode::ActionList;
    lv_obj_clean(content_parent_);

    for (size_t i = 0; i < ACTION_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        action_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_action_list();
}

void BackupScreen::render_action_list()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ACTION_COUNT; ++i) {
        const bool is_selected = (i == selected_action_);
        const bool is_confirming = is_selected && static_cast<Action>(i) == Action::FormatSdCard
                                    && format_confirm_pending_;
        lv_obj_set_style_text_color(
            action_labels_[i], is_confirming ? pal.error : (is_selected ? pal.accent : pal.primary_text), 0);
        if (is_confirming) {
            lv_label_set_text_fmt(action_labels_[i], "> %s -- confirm?", ACTION_NAMES[i]);
        } else {
            lv_label_set_text_fmt(action_labels_[i], "%s%s", is_selected ? "> " : "", ACTION_NAMES[i]);
        }
    }

    if (action_labels_[selected_action_] != nullptr) {
        lv_obj_scroll_to_view(action_labels_[selected_action_], LV_ANIM_ON);
    }
}

void BackupScreen::move_action_selection(int32_t delta)
{
    int32_t index = static_cast<int32_t>(selected_action_) + delta;
    const int32_t count = static_cast<int32_t>(ACTION_COUNT);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_action_ = static_cast<size_t>(index);

    format_confirm_pending_ = false;

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render_action_list();
}

void BackupScreen::activate_action()
{
    switch (static_cast<Action>(selected_action_)) {
        case Action::CreateBackup: {
            char filename[32];
            if (vault::backup::create_backup(filename, sizeof(filename))) {
                ESP_LOGI(TAG, "Backup created: %s", filename);
                lv_label_set_text_fmt(status_label_, "Created: %s", filename);
            } else {
                ESP_LOGW(TAG, "Create backup failed");
                lv_label_set_text(status_label_, "Backup failed (locked or no SD card?)");
            }
            return;
        }

        case Action::RestoreBackup:
            enter_backup_list();
            return;

        case Action::ExportVault: {
            const security::permission::Result perm =
                security::permission::check(security::permission::Operation::ExportVault);
            if (perm != security::permission::Result::Allowed) {
                ESP_LOGI(TAG, "Export Vault denied (%d)", static_cast<int>(perm));
                lv_label_set_text(status_label_, "Not allowed");
                return;
            }

            char filename[32];
            if (vault::csv::export_csv(filename, sizeof(filename))) {
                ESP_LOGI(TAG, "Vault exported: %s", filename);
                lv_label_set_text_fmt(status_label_, "Exported: %s", filename);
            } else {
                ESP_LOGW(TAG, "Export vault failed");
                lv_label_set_text(status_label_, "Export failed (locked or no SD card?)");
            }
            return;
        }

        case Action::ImportVault:
            enter_import_list();
            return;

        case Action::RefreshSdCard: {
            if (storage::refresh_sdcard()) {
                uint64_t total = 0;
                uint64_t used = 0;
                if (storage::sd::get_usage(total, used)) {
                    const unsigned used_mb = static_cast<unsigned>(used / (1024 * 1024));
                    const unsigned total_mb = static_cast<unsigned>(total / (1024 * 1024));
                    lv_label_set_text_fmt(status_label_, "SD card mounted: %u / %u MB", used_mb, total_mb);
                } else {
                    lv_label_set_text(status_label_, "SD card mounted");
                }
            } else if (storage::sd::mount_looked_unreadable()) {
                // See storage::sd::mount_looked_unreadable()'s own
                // comment -- this is a best-effort signal, not a
                // guaranteed-precise diagnosis, but it's more useful
                // than a flat "not inserted" when it applies.
                lv_label_set_text(status_label_, "Card found but unreadable -- try Format SD Card");
            } else {
                lv_label_set_text(status_label_, "No SD card detected");
            }
            return;
        }

        case Action::FormatSdCard: {
            if (!format_confirm_pending_) {
                format_confirm_pending_ = true;
                render_action_list();
                lv_label_set_text(status_label_, "This erases everything on the card. Press OK again to confirm.");
                return;
            }

            format_confirm_pending_ = false;
            ESP_LOGW(TAG, "Formatting SD card (user-requested)");
            if (storage::format_sdcard()) {
                lv_label_set_text(status_label_, "SD card formatted and mounted");
            } else {
                lv_label_set_text(status_label_, "Format failed -- no card, or a hardware fault");
            }
            render_action_list();
            return;
        }
    }
}

void BackupScreen::enter_backup_list()
{
    const security::permission::Result perm =
        security::permission::check(security::permission::Operation::RestoreBackup);
    if (perm != security::permission::Result::Allowed) {
        ESP_LOGI(TAG, "Restore Backup denied (%d)", static_cast<int>(perm));
        lv_label_set_text(status_label_, "Not allowed");
        return;
    }

    mode_ = Mode::BackupList;
    selected_backup_ = 0;
    restore_confirm_pending_ = false;
    build_backup_list();
}

void BackupScreen::build_backup_list()
{
    lv_obj_clean(content_parent_);

    backup_count_ = vault::backup::list_backups(backups_, MAX_BACKUPS);

    const theme::Palette& pal = theme::current();

    if (backup_count_ == 0) {
        lv_obj_t* empty = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(empty, pal.secondary_text, 0);
        lv_label_set_text(empty, "No backups found");
        lv_obj_center(empty);
    } else {
        for (size_t i = 0; i < backup_count_; ++i) {
            lv_obj_t* label = lv_label_create(content_parent_);
            lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
            backup_labels_[i] = label;
        }
    }

    status_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_backup_list();
}

void BackupScreen::render_backup_list()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < backup_count_; ++i) {
        const bool is_selected = (i == selected_backup_);
        const bool is_confirming = is_selected && restore_confirm_pending_;

        lv_obj_set_style_text_color(
            backup_labels_[i], is_confirming ? pal.error : (is_selected ? pal.accent : pal.primary_text), 0);

        const unsigned kb = static_cast<unsigned>(backups_[i].size_bytes / 1024);
        if (is_confirming) {
            lv_label_set_text_fmt(backup_labels_[i], "> %s (%uKB) -- confirm?", backups_[i].filename, kb);
        } else {
            lv_label_set_text_fmt(backup_labels_[i], "%s%s (%uKB)", is_selected ? "> " : "",
                                   backups_[i].filename, kb);
        }
    }

    if (backup_count_ > 0) {
        lv_obj_scroll_to_view(backup_labels_[selected_backup_], LV_ANIM_ON);
    }
}

void BackupScreen::move_backup_selection(int32_t delta)
{
    if (backup_count_ == 0) {
        return;
    }

    restore_confirm_pending_ = false;

    int32_t index = static_cast<int32_t>(selected_backup_) + delta;
    const int32_t count = static_cast<int32_t>(backup_count_);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_backup_ = static_cast<size_t>(index);

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render_backup_list();
}

void BackupScreen::activate_backup()
{
    if (backup_count_ == 0) {
        return;
    }

    if (!restore_confirm_pending_) {
        restore_confirm_pending_ = true;
        render_backup_list();
        lv_label_set_text(status_label_, "This will overwrite the current vault. Press OK again to confirm.");
        return;
    }

    const char* filename = backups_[selected_backup_].filename;
    ESP_LOGI(TAG, "Restoring backup: %s", filename);

    if (!vault::backup::restore_backup(filename)) {
        restore_confirm_pending_ = false;
        ESP_LOGE(TAG, "Restore failed");
        lv_label_set_text(status_label_, "Restore failed");
        render_backup_list();
        return;
    }

    ESP_LOGW(TAG, "Restore complete -- restarting now");
    lv_label_set_text(status_label_, "Restored. Restarting...");
    esp_restart(); // does not return
}

void BackupScreen::enter_import_list()
{
    mode_ = Mode::ImportList;
    selected_import_ = 0;
    build_import_list();
}

void BackupScreen::build_import_list()
{
    lv_obj_clean(content_parent_);

    import_file_count_ = vault::csv::list_import_files(import_files_, MAX_IMPORT_FILES);

    const theme::Palette& pal = theme::current();

    if (import_file_count_ == 0) {
        lv_obj_t* empty = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(empty, pal.secondary_text, 0);
        lv_label_set_text(empty, "No files in /sdcard/vault/import");
        lv_obj_center(empty);
    } else {
        for (size_t i = 0; i < import_file_count_; ++i) {
            lv_obj_t* label = lv_label_create(content_parent_);
            lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
            import_labels_[i] = label;
        }
    }

    status_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_import_list();
}

void BackupScreen::render_import_list()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < import_file_count_; ++i) {
        const bool is_selected = (i == selected_import_);
        lv_obj_set_style_text_color(import_labels_[i], is_selected ? pal.accent : pal.primary_text, 0);

        const unsigned kb = static_cast<unsigned>(import_files_[i].size_bytes / 1024);
        lv_label_set_text_fmt(import_labels_[i], "%s%s (%uKB)", is_selected ? "> " : "",
                               import_files_[i].filename, kb);
    }

    if (import_file_count_ > 0) {
        lv_obj_scroll_to_view(import_labels_[selected_import_], LV_ANIM_ON);
    }
}

void BackupScreen::move_import_selection(int32_t delta)
{
    if (import_file_count_ == 0) {
        return;
    }

    int32_t index = static_cast<int32_t>(selected_import_) + delta;
    const int32_t count = static_cast<int32_t>(import_file_count_);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_import_ = static_cast<size_t>(index);

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render_import_list();
}

void BackupScreen::activate_import()
{
    if (import_file_count_ == 0) {
        return;
    }

    // NOT destructive (only ADDS entries -- see vault_csv.hpp), so no
    // press-twice confirm the way Restore Backup needs one.
    const char* filename = import_files_[selected_import_].filename;
    ESP_LOGI(TAG, "Importing CSV: %s", filename);

    const vault::csv::ImportResult result = vault::csv::import_csv(filename);
    lv_label_set_text_fmt(status_label_, "Imported %u, skipped %u", static_cast<unsigned>(result.created),
                           static_cast<unsigned>(result.skipped));
}

bool BackupScreen::on_input(InputAction action)
{
    if (mode_ == Mode::BackupList) {
        switch (action) {
            case InputAction::RotateLeft:
                move_backup_selection(-1);
                return true;

            case InputAction::RotateRight:
                move_backup_selection(+1);
                return true;

            case InputAction::OkShort:
                activate_backup();
                return true;

            case InputAction::BackShort:
                mode_ = Mode::ActionList;
                build_action_list();
                return true;

            default:
                return false;
        }
    }

    if (mode_ == Mode::ImportList) {
        switch (action) {
            case InputAction::RotateLeft:
                move_import_selection(-1);
                return true;

            case InputAction::RotateRight:
                move_import_selection(+1);
                return true;

            case InputAction::OkShort:
                activate_import();
                return true;

            case InputAction::BackShort:
                mode_ = Mode::ActionList;
                build_action_list();
                return true;

            default:
                return false;
        }
    }

    switch (action) {
        case InputAction::RotateLeft:
            move_action_selection(-1);
            return true;

        case InputAction::RotateRight:
            move_action_selection(+1);
            return true;

        case InputAction::OkShort:
            activate_action();
            return true;

        case InputAction::BackShort:
            return false; // pop back to MainMenu

        default:
            return false;
    }
}

} // namespace ui::screens
