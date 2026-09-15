#pragma once

#include "ui/screen.hpp"

#include "vault/vault_backup.hpp"
#include "vault/vault_csv.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::BackupScreen
//
// docs/GUI.md section 15: Create Backup / Restore Backup / Export
// Vault / Import Vault.
//
// Refresh SD Card / Format SD Card added at the project owner's
// request, not part of GUI.md 15 -- this board has no card-detect
// GPIO (see storage::sd's own file comment), so a card inserted
// after boot never gets noticed on its own; Refresh calls
// storage::refresh_sdcard() to retry. Format is for a card that LOOKS
// present but shows as unreadable -- commonly because it shipped
// pre-formatted exFAT (typical for 32GB+ cards) and this project's
// FAT-only mount code can't read that. Destructive (erases the
// card), press-twice confirm, same pattern as Restore Backup below.
//
// Create Backup and Restore Backup: call
// vault::backup::create_backup()/restore_backup() -- raw copies of
// this project's own internal vault.db, only ever readable by
// another KeyKeeper2 (see vault_backup.hpp).
//
// Export Vault and Import Vault: call vault::csv::export_csv()/
// import_csv() -- plain CSV, for interop with OTHER password
// managers/apps (see vault_csv.hpp for the exact format and the
// lenient header-name matching on import). Export requires
// security::permission::check(Operation::ExportVault) (GUI.md 15's
// "protected operations are checked via SecurityService" -- this
// reveals every stored password in the clear to a file). Import picks
// a file the same way Restore Backup does (a list from
// vault::csv::list_import_files()), but is NOT destructive -- it only
// ADDS entries, so there's no press-twice confirm for it the way
// Restore has.
//
// Restore Backup: selecting it shows the list of existing backup
// files (vault::backup::list_backups()); picking one runs
// security::permission::check(Operation::RestoreBackup) (GUI.md 15:
// "protected operations are checked via SecurityService"), then a
// press-twice confirm (same pattern as AccountViewScreen's Delete --
// this is destructive and overwrites the current vault). On success,
// the device restarts via esp_restart() -- required because
// vault::backup::restore_backup() only replaces the on-disk vault.db,
// it does not (and safely cannot, see vault_backup.hpp) reload
// VaultRepository's in-RAM cache.
//
// GUI.md 15 also asks for a progress indicator + non-blocking
// behavior for long operations. Not implemented -- create/restore
// here are large-file-copy operations only (not slow enough on this
// vault's expected size to justify it yet), and both currently run
// synchronously on the UI task, briefly blocking input while they
// run. Flagged, not solved.
// =============================================================================

namespace ui::screens {

class BackupScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Action : uint8_t
    {
        CreateBackup,
        RestoreBackup,
        ExportVault,
        ImportVault,
        RefreshSdCard,
        FormatSdCard,
    };
    static constexpr size_t ACTION_COUNT = 6;

    enum class Mode : uint8_t
    {
        ActionList,
        BackupList,
        ImportList,
    };

    void build_action_list();
    void render_action_list();
    void move_action_selection(int32_t delta);
    void activate_action();

    void enter_backup_list();
    void build_backup_list();
    void render_backup_list();
    void move_backup_selection(int32_t delta);
    void activate_backup();

    void enter_import_list();
    void build_import_list();
    void render_import_list();
    void move_import_selection(int32_t delta);
    void activate_import();

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* status_label_ = nullptr;

    Mode mode_ = Mode::ActionList;

    lv_obj_t* action_labels_[ACTION_COUNT]{};
    size_t selected_action_ = 0;
    bool format_confirm_pending_ = false; // FormatSdCard is destructive -- press-twice, same pattern as restore

    static constexpr size_t MAX_BACKUPS = 16;
    vault::backup::BackupInfo backups_[MAX_BACKUPS]{};
    size_t backup_count_ = 0;
    lv_obj_t* backup_labels_[MAX_BACKUPS]{};
    size_t selected_backup_ = 0;
    bool restore_confirm_pending_ = false;

    static constexpr size_t MAX_IMPORT_FILES = 16;
    vault::csv::ImportFileInfo import_files_[MAX_IMPORT_FILES]{};
    size_t import_file_count_ = 0;
    lv_obj_t* import_labels_[MAX_IMPORT_FILES]{};
    size_t selected_import_ = 0;
};

} // namespace ui::screens
