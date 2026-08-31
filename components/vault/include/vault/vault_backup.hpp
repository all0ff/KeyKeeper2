#pragma once

#include <cstddef>
#include <cstdint>

// =============================================================================
// vault::backup
//
// docs/GUI.md section 15 ("Backup Screens"): Create Backup / Restore
// Backup. Export/Import Vault are NOT implemented here -- they imply
// a different interchange format (storage_paths.hpp's own comment:
// "e.g. CSV from another password manager"), a separate, unscoped
// design decision deferred for now -- see components/ui's
// BackupScreen for how that's surfaced to the user.
//
// Backups are numbered by a monotonic counter, not a real date/time
// -- there is no RTC or NTP source on this device (same limitation
// already documented for TOTP in vault_model.hpp). Filenames look
// like "vault_001.bak", "vault_002.bak", ...
//
// Like everything else in vault::, create/list/delete require the
// device to be Unlocked (security::lock::state() ==
// State::Unlocked), the same baseline gate vault.hpp already applies
// everywhere. Restore additionally requires
// security::permission::check(Operation::RestoreBackup) -- checked by
// the CALLER (ui::screens::BackupScreen), not here, so this module
// doesn't need to depend on security's permission layer for something
// the GUI already checks before calling in (matches vault.hpp's own
// stated reasoning for not calling permission::check() itself).
//
// IMPORTANT: restore_backup() overwrites vault.db on disk but does
// NOT reload VaultRepository's in-RAM cache (see
// vault_repository.hpp's "load once, mutate, save whole" design) --
// the caller MUST restart the device after a successful restore, or
// the running firmware and the file on disk will disagree.
// ui::screens::BackupScreen does this via esp_restart().
// =============================================================================

namespace vault::backup {

struct BackupInfo
{
    char filename[32];
    size_t size_bytes;
};

/**
 * @brief Copy the current vault.db to a new numbered file under
 *        storage::paths::VAULT_BACKUP_DIR.
 *
 * Requires the vault to be Unlocked and a microSD card to be present.
 *
 * @param out_filename Receives the created file's name (not full
 *                      path). May be nullptr if not needed.
 * @return true on success.
 */
bool create_backup(char* out_filename, size_t out_filename_size);

/**
 * @brief List existing backup files, most recently created first.
 *
 * @return Number of entries written to out (<= max_count).
 */
size_t list_backups(BackupInfo* out, size_t max_count);

/**
 * @brief Replace vault.db with the contents of an existing backup
 *        file.
 *
 * DESTRUCTIVE -- overwrites the current vault entirely, with no
 * further confirmation at this layer (the caller is responsible for
 * confirming with the user first, and for restarting the device
 * afterward -- see the file-level comment).
 *
 * @return true on success. vault.db is left untouched on failure.
 */
bool restore_backup(const char* filename);

bool delete_backup(const char* filename);

} // namespace vault::backup
