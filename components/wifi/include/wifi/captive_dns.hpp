#pragma once

#include "esp_netif.h"

// =============================================================================
// wifi::captive_dns -- the DNS half of a captive portal.
//
// While Access Point mode is running, a phone or laptop that joins
// this device's network still tries to resolve normal internet
// domain names (and, right after joining, the specific "is this
// network actually connected to the internet" probe domains every
// modern OS checks automatically -- Apple's captive.apple.com,
// Android's connectivitycheck.gstatic.com, Windows'
// www.msftconnecttest.com, ...). This component answers EVERY DNS
// query it receives with THIS device's own AP IP address, regardless
// of what name was actually asked for -- there is nowhere else for
// those queries to go anyway (the AP has no uplink to the real
// internet), and answering this way is what makes the OS's own probe
// fail in a way it recognizes as "this network wants you to sign in
// first", which is what makes it pop the captive-portal browser open
// automatically instead of the person having to know to type in
// 192.168.4.1 themselves.
//
// The OTHER half is on the HTTP side -- see web_service.cpp's
// wildcard catch-all handler, which is what actually gets requested
// once DNS has pointed those probe domains here.
//
// Deliberately narrow: this is NOT a real DNS server (no caching, no
// forwarding, no recursion, no records besides a single A answer) --
// it only exists to make Access Point mode self-explanatory to
// connect to, not to provide DNS service. Must only run while AP mode
// is actually up; wifi_service.cpp is responsible for calling
// start()/stop() around AP start/stop, since hijacking DNS while
// Station mode is connected to a real network would break normal
// browsing on it.
// =============================================================================

namespace wifi::captive_dns {

/**
 * @brief Start the DNS hijack task, answering every query with
 *        ap_netif's own current IP (queried once, at start() time --
 *        this device's own AP address doesn't change while the AP is
 *        up, so no need to re-query per packet).
 *
 * Safe to call again if already running (restarts).
 *
 * @return false if ap_netif has no IP yet, or the UDP socket/task
 *         couldn't be created.
 */
bool start(esp_netif_t* ap_netif);

/// No-op if not running.
void stop();

bool is_running();

} // namespace wifi::captive_dns
