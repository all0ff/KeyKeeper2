#pragma once

#include <cstddef>
#include <cstdint>

// =============================================================================
// vault::csv -- CSV export/import for interop with OTHER password
// managers/apps, as opposed to vault::backup (vault_backup.hpp),
// which is a raw copy of this project's own internal vault.db binary
// format and is only ever readable by another KeyKeeper2. This is
// what storage_paths.hpp's own comment on VAULT_IMPORT_DIR/
// VAULT_EXPORT_DIR meant by "e.g. CSV from another password manager"
// -- deferred at the time, built now.
//
// Format: plain CSV (RFC 4180-style quoting: a field containing a
// comma, double quote, or newline is wrapped in double quotes, with
// internal double quotes doubled), one header row naming every
// vault::VaultEntry field, one row per entry after that:
//
//   login,password,url,notes,totp_secret,category,favorite
//
// No single CSV shape is universally standard across password
// managers (Bitwarden, Chrome, 1Password, KeePass, ... each use
// their own column names), so rather than chasing several
// vendor-specific formats, export uses ONE clearly-labeled, widely
// parseable shape, and import is lenient about the header names it
// accepts on the way in -- see csv_column_from_header() in
// vault_csv.cpp for the exact recognized synonyms (e.g. "username" or
// "user" both map to login, "site" or "website" map to url). This
// covers the common case (a spreadsheet edit, or another app's own
// CSV export with similar-enough column names) without trying to
// special-case every vendor's exact schema.
//
// Export requires security::permission::check(Operation::ExportVault)
// -- checked by the CALLER (ui::screens::BackupScreen), not here,
// matching vault_backup.hpp's own reasoning for RestoreBackup. Import
// has no equivalent gated Operation (see permission_manager.hpp) --
// it only ADDS entries via vault::create_entry(), same baseline
// Unlocked gate as any other vault:: write, nothing more sensitive
// than that.
//
// Import does NOT replace the vault (unlike vault::backup::restore_backup())
// -- it ADDS the rows it can parse as new entries, leaving whatever's
// already there untouched. A row vault::validate() rejects (a field
// too long, an empty login, ...) is skipped, not fatal to the rest of
// the import -- see import_csv()'s own doc comment for how failures
// are reported.
// =============================================================================

namespace vault::csv {

/**
 * @brief Write every vault entry to storage::paths::VAULT_EXPORT_DIR
 *        as one CSV file. Overwrites any previous export at that
 *        exact filename -- export is "give me the current full list
 *        right now", not something to keep multiple dated copies of
 *        the way backups are.
 *
 * Requires the vault to be Unlocked and a microSD card present, same
 * as vault::backup::create_backup().
 *
 * @param out_filename Receives the written file's name (not full
 *                      path). May be nullptr if not needed.
 * @return true on success.
 */
bool export_csv(char* out_filename, size_t out_filename_size);

struct ImportResult
{
    size_t created = 0; ///< Rows successfully added as new entries.
    size_t skipped = 0; ///< Rows that failed vault::validate() (too long, empty login, ...) or were malformed CSV.
};

/**
 * @brief Read a CSV file from storage::paths::VAULT_IMPORT_DIR and
 *        create a new vault entry for each row that parses and
 *        validates successfully.
 *
 * @param filename Just the file name (not full path), as returned by
 *                  list_import_files().
 */
ImportResult import_csv(const char* filename);

struct ImportFileInfo
{
    char filename[32];
    size_t size_bytes;
};

/// List files sitting in storage::paths::VAULT_IMPORT_DIR, for the UI
/// to offer a pick-a-file list before calling import_csv() -- same
/// shape as vault::backup::list_backups().
size_t list_import_files(ImportFileInfo* out, size_t max_count);

} // namespace vault::csv
