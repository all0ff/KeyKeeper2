#pragma once

#include "esp_http_server.h"

// =============================================================================
// web (internal) -- REST endpoints for vault BACKUP (SD-card .bak
// files, vault::backup -- a raw copy of this project's own internal
// vault.db format, distinct from Export/Import Vault's CSV, which
// isn't exposed over REST yet).
//
// Every route here requires the device to be Unlocked
// (security::lock::state()), same shared-session model as
// web_service.cpp's login endpoint. Restore ADDITIONALLY requires
// security::permission::check(Operation::RestoreBackup) -- checked
// HERE (the caller), matching vault_backup.hpp's own stated reason
// for not checking it internally (ui::screens::BackupScreen already
// checks it on-device for the exact same reason: the module doesn't
// want a dependency on security's permission layer for something
// every caller already needs to check anyway).
//
// Restore is DESTRUCTIVE and requires a device restart to actually
// take effect (see vault_backup.hpp's own file comment -- restoring
// overwrites vault.db on disk but does NOT reload
// VaultRepository's in-RAM cache) -- handle_restore_backup() calls
// esp_restart() itself after a successful restore, same as
// ui::screens::BackupScreen does. The HTTP response is sent
// BEFORE that restart, warning the browser to expect the connection
// to drop.
// =============================================================================

namespace web {

/// Registers all backup routes on an already-started httpd server.
/// Called once from web::start().
void register_backup_routes(httpd_handle_t server);

} // namespace web
