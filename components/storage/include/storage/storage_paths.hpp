#pragma once

// =============================================================================
// storage::paths
//
// Single source of truth for where things live. Two physical backends,
// chosen deliberately per path:
//
//   - Internal LittleFS ("/data"): holds vault.db only. This is the
//     one thing that must ALWAYS be available regardless of whether a
//     microSD card is inserted -- the vault has to work with no card
//     in the slot.
//   - microSD ("/sdcard"): holds backup/import/export. These are
//     bulk, user-facing operations (pull the card, read it on a PC),
//     so removable media is the natural fit. They are NOT available
//     if no card is inserted -- see storage::status().sdcard_present.
//
// This mapping (which of the 4 plan paths goes on which backend) was
// not specified in the build plan beyond the path strings themselves;
// it's a design decision made here and documented for that reason.
//
// Once storage::init() has mounted both backends, any component can
// just use plain POSIX stdio (fopen/fread/fwrite/fclose) against
// these paths -- that's the point of ESP-IDF's VFS mount points, no
// extra wrapper API needed on top.
// =============================================================================

namespace storage::paths {

inline constexpr const char* INTERNAL_MOUNT_POINT = "/data";
inline constexpr const char* SDCARD_MOUNT_POINT = "/sdcard";

/// The vault database itself. Internal LittleFS -- always available.
inline constexpr const char* VAULT_DB = "/data/vault/vault.db";

/// Backup archives written by the vault backup feature. microSD.
inline constexpr const char* VAULT_BACKUP_DIR = "/sdcard/vault/backup";

/// Files staged for import into the vault (e.g. CSV from another
/// password manager). microSD, so the user can copy files onto the
/// card from a PC before inserting it.
inline constexpr const char* VAULT_IMPORT_DIR = "/sdcard/vault/import";

/// Files exported from the vault for the user to take with them.
/// microSD, so the user can pull the card and read them on a PC.
inline constexpr const char* VAULT_EXPORT_DIR = "/sdcard/vault/export";

} // namespace storage::paths
