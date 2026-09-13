#pragma once

#include <cstdint>

// =============================================================================
// web -- WebService (docs/WEB.md, Phase B)
//
// This component: an HTTP server, PIN-based login (this file), and
// REST CRUD for individual vault entries (web_vault_routes.cpp, an
// implementation-internal file -- see its own comment for the exact
// routes). NOT built yet: search over REST, backup/restore/settings
// over REST (WEB.md section 7's full "Supported Operations" list),
// serving a real Web UI beyond the bare login page, or Captive
// Portal. Those are separate, later increments.
//
// SHARED SESSION MODEL (a deliberate choice, not a default): logging
// in via the web calls security::lock::unlock() directly -- the SAME
// unlock the on-device LockScreen uses. There is no separate
// "web session" concept layered on top (security::session's own
// Origin::WebUi enum value exists but isn't used by this component --
// see session_manager.hpp's comment that begin_session()/end_session()
// aren't meant to be called from outside components/security).
// Consequences worth being explicit about:
//   - If the device is already Unlocked (someone at the physical
//     screen unlocked it), any web client on the same network can
//     already use authenticated endpoints without ever submitting a
//     PIN via the web login form -- there is no separate,
//     web-specific gate. Confirmed as acceptable for this project.
//   - Logging in via web actually unlocks the physical device --
//     its screen will show Unlocked even though nobody touched it.
//   - There's no logout endpoint: locking (BackLong on MainMenu,
//     auto-lock timeout) already ends the session everywhere, since
//     it's the same global state.
//   - A wrong PIN via the web login counts against the SAME staged
//     lockout/wipe counter as on-device attempts (security::pin).
//     Reaching the threshold via THIS endpoint does NOT trigger the
//     vault/PIN wipe ui::screens::LockScreen performs for an
//     on-device failure -- it disables Wi-Fi instead (persisted via
//     settings::set_wifi(), so it stays off across reboots until
//     re-enabled from WifiSettingsScreen). A remote brute-force
//     attempt doesn't need a destructive, irreversible response the
//     way repeated physical-device guesses might; cutting off the
//     remote attack surface is proportionate and reversible. See
//     web_service.cpp's login handler.
//
// SECURITY NOTE: the login endpoint sends the PIN in a plain HTTP
// POST body -- no TLS. WEB.md's own Authentication & Security section
// explicitly defers HTTPS ("на текущем этапе... в доверенной локальной
// сети"). This means the PIN is transmitted in the clear on whatever
// network the device's Wi-Fi is on. Same class of exposure already
// documented for vault.db/Wi-Fi credentials pre-Flash-Encryption, but
// worth restating here since it's specifically a credential in
// transit, not just at rest.
// =============================================================================

namespace web {

/// IDs for event_bus::Category::Web. WEB.md section 10 also lists
/// ApiRequestReceived/ClientConnected/ClientDisconnected -- not
/// implemented in this increment (esp_http_server doesn't expose
/// raw connect/disconnect at the HTTP layer without more plumbing
/// than this step's scope justifies).
enum class WebEventId : uint32_t
{
    ServerStarted,
    ServerStopped,
    LoginSucceeded,
    LoginFailed,
};

/// Must be called once, after settings::init()/event_bus::init(). Does
/// not start the HTTP server itself -- call start() next.
bool init();

bool is_initialized();

/**
 * @brief Start the HTTP server and register its routes.
 *
 * Safe to call whether or not Wi-Fi currently has a usable IP --
 * esp_http_server will bind and simply become reachable once a
 * network interface comes up (this project doesn't currently wait
 * for a WifiEventId::Connected/ApStarted event before starting the
 * server; a reasonable simplification for this increment, not a
 * considered decision that this ordering never matters).
 */
bool start();

void stop();

bool is_running();

} // namespace web
