#pragma once

#include "esp_http_server.h"

// =============================================================================
// web (internal) -- REST endpoints for the vault itself.
//
// docs/WEB.md section 7's "Supported Operations" list for the vault:
// list/view/create/edit/delete/search. GET /api/v1/search?q=... now
// covers search -- see handle_search()'s own comment, shares its
// matching logic with ui::screens::SearchScreen via
// vault::search_entries(), not a separate copy. Backup/restore over
// REST live in web_backup_routes.hpp/.cpp instead (a different
// domain -- SD-card files, not vault entries); settings over REST are
// in web_settings_routes.hpp/.cpp.
//
// Every route here requires the device to be Unlocked
// (security::lock::state()), same shared-session model as
// web_service.cpp's login endpoint -- see that file's header comment
// for what that does and doesn't mean. No additional
// security::permission::check() calls beyond that: matches
// vault::VaultService's own stance (see vault.hpp's file comment)
// that plain CRUD isn't one of the six specifically-gated
// operations.
//
// JSON entry shape (both directions):
//   {"id":1,"login":"...","password":"...","url":"...","notes":"...",
//    "totp_secret":"...","category":"...","favorite":false,
//    "created_at":0,"updated_at":0}
// GET /api/v1/entries (the list endpoint) omits password/notes/
// totp_secret/created_at/updated_at to keep the response small for a
// vault with many entries -- adds "has_otp" (bool) instead of the raw
// totp_secret. GET /api/v1/entry?id=N (single entry) returns every
// field.
// =============================================================================

namespace web {

/// Registers all vault CRUD routes on an already-started httpd server.
/// Called once from web::start(), right after the auth routes.
void register_vault_routes(httpd_handle_t server);

} // namespace web
