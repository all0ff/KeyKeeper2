#pragma once

#include "esp_http_server.h"

// =============================================================================
// web (internal) -- REST endpoints for device settings.
//
// GET /api/v1/settings returns all editable sections in one response;
// PUT /api/v1/settings/<section> replaces one section, same
// "read-modify-write the WHOLE section" shape as the on-device
// Settings screens' own Save button (not a partial patch -- a field
// omitted from the PUT body is written back with ITS CURRENT value,
// not cleared, by reading settings::all() first and only overwriting
// fields the JSON body actually contains -- see json_get_uint32()'s
// own comment in web_json_helpers.hpp for how that's done for numeric
// fields specifically).
//
// Sections: general, usb, security, wifi. NOT exposed here, on
// purpose:
//   - settings::GeneralSettings::theme -- only one value exists right
//     now (settings::Theme::Dark), nothing to change; included in the
//     GET response as read-only information, rejected (ignored) on PUT.
//   - security.pin_length / changing the PIN itself -- stays a
//     device-only action (ui::screens::SecuritySettingsScreen's
//     Change PIN flow). Remote PIN changes over plain HTTP (no TLS)
//     are a materially different risk than changing e.g. auto-lock
//     timing, and the whole async-verify UX is tightly coupled to
//     on-device widgets::PinEntry anyway.
//   - security.secret_word -- changing this via the web would
//     immediately break the CURRENT web session's own routing (the
//     server restarts with a new path prefix baked into every route,
//     see web_json_helpers.hpp's build_prefixed_path()), which is a
//     confusing way to lock yourself out of the very page you're
//     using to change it. Stays device-only
//     (ui::screens::WifiSettingsScreen).
//   - Factory Reset -- destructive, no reason to expose remotely;
//     stays device-only (ui::screens::SecuritySettingsScreen).
//
// Same auth model as every other route here: requires Unlocked, no
// additional security::permission::check() (settings::SecuritySettings::
// web_ui_permissions' WEB_UI_CHANGE_SETTINGS bit exists but isn't
// enforced anywhere yet, on-device or here -- consistent with how the
// vault CRUD routes don't check web_ui_permissions either; see
// web_vault_routes.hpp's own comment).
//
// PUT /api/v1/settings/wifi also calls wifi::apply_settings()
// afterward, matching WifiSettingsScreen's own Save -- the whole
// point of changing Wi-Fi settings is to reconnect right away, not
// just persist a value nothing acts on until next boot.
// =============================================================================

namespace web {

/// Registers all settings routes on an already-started httpd server.
/// Called once from web::start(), alongside the auth and vault routes.
void register_settings_routes(httpd_handle_t server);

} // namespace web
