#pragma once

#include <cstdint>

// =============================================================================
// wifi -- WiFiService (docs/developer/07_wifi.md)
//
// Single point of control for networking, per that document's own
// stated principles: no other component should call the ESP-IDF Wi-Fi
// API directly, state changes are published on event_bus (not
// polled), and configuration lives in settings::WifiSettings (see
// settings_types.hpp) -- this component reads it, never a separate
// copy of its own.
//
// Supported modes (settings::WifiMode): Disabled, Station (connect to
// an existing network), AccessPoint (host one). Both at once
// (AP+STA) is explicitly future work per 07_wifi.md, not implemented.
//
// WiFiService does NOT implement HTTP, a Web UI, or any application-
// level security decision (07_wifi.md's own "Responsibilities" list
// excludes all of that) -- it only brings up/tears down the network
// link itself. WebService (not built yet -- see docs/WEB.md) is the
// next layer up, meant to sit on top of whatever this component
// brings up.
//
// init() sets up the underlying ESP-IDF Wi-Fi/netif/event-loop
// machinery once at boot but does not itself start any radio mode --
// call apply_settings() after that (and again any time
// settings::WifiSettings changes) to actually bring the configured
// mode up. This mirrors how e.g. components/display separates
// hardware bring-up from applying a specific configuration.
// =============================================================================

namespace wifi {

enum class ConnectionState : uint8_t
{
    Idle,        ///< mode == Disabled, or init() succeeded but apply_settings() hasn't run yet.
    Connecting,  ///< Station mode, association/DHCP in progress.
    Connected,   ///< Station mode, has an IP address.
    Disconnected,///< Station mode, lost or never established a connection (see last_error()).
    ApRunning,   ///< Access Point mode, radio up and accepting clients.
    Failed,      ///< Station mode, gave up retrying (see last_error()).
};

/// IDs for event_bus::Category::Wifi -- see event_bus/event.hpp's
/// file comment for why each category owns its own event id enum.
enum class WifiEventId : uint32_t
{
    Started,          ///< init() completed.
    Stopped,          ///< stop() completed, or apply_settings() switched to Disabled.
    Connecting,       ///< Station mode: association attempt started.
    Connected,        ///< Station mode: got an IP address. Payload: none (see ip_address()).
    Disconnected,     ///< Station mode: lost an established connection.
    ConnectionFailed, ///< Station mode: gave up retrying.
    ApStarted,        ///< Access Point mode: radio up.
    ApStopped,        ///< Access Point mode: radio down.
    ApClientJoined,   ///< Access Point mode: a client associated. Payload: current client count.
    ApClientLeft,     ///< Access Point mode: a client disassociated. Payload: current client count.
};

/**
 * @brief Bring up the underlying ESP-IDF Wi-Fi/netif machinery.
 *
 * Must be called once, after settings::init() and event_bus::init().
 * Does not start any radio mode by itself -- call apply_settings()
 * next (typically right away, so a Disabled/Station/AccessPoint
 * configuration from a previous session takes effect at boot the same
 * way it would after the user changes it later).
 */
bool init();

bool is_initialized();

/**
 * @brief (Re)apply settings::all().wifi -- switch mode, reconnect
 *        with new credentials, or stop the radio for Disabled.
 *
 * Safe to call repeatedly (e.g. every time the user saves the WiFi
 * settings screen) -- tears down whatever was previously running
 * first.
 *
 * @return true if the requested mode was successfully started (for
 *         Station, this means the connection attempt was started
 *         successfully, NOT that it has necessarily succeeded yet --
 *         watch state() or subscribe to WifiEventId::Connected/
 *         ConnectionFailed for the outcome).
 */
bool apply_settings();

/// Equivalent to setting mode = Disabled and calling apply_settings().
void stop();

ConnectionState state();

/**
 * @brief Current IP address as a dotted-decimal string, valid only
 *        when state() is Connected or ApRunning.
 *
 * @return Pointer to a static buffer, valid until the next call to
 *         this function from the same task. Empty string otherwise.
 */
const char* ip_address();

/// Meaningful only in ApRunning state; 0 otherwise.
uint8_t ap_client_count();

/// Human-readable reason for the most recent Disconnected/Failed
/// transition (e.g. "wrong password", "AP not found") -- best-effort,
/// not guaranteed for every ESP-IDF Wi-Fi error code. Empty if nothing
/// has failed yet this session.
const char* last_error();

} // namespace wifi
