#pragma once

#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"

// =============================================================================
// blelink -- a message pipe between the vault and its dongle over BLE (NimBLE)
//
// The dongle is the GATT peripheral: it advertises one service with two characteristics
// (RX: write without response, TX: notify). The vault is the central: it scans, connects, negotiates
// the MTU, subscribes to TX. A message (one kkproto frame, up to 256 bytes) is cut into ATT-sized
// fragments (frag.hpp) and put together again on the other side.
//
// There is NO BLE-level pairing or encryption: the Noise channel of kkproto on top of this pipe is the
// security. What an eavesdropper sees is the radio address and ciphertext.
//
// Threading: send(), recv() and the commands below are called from ONE owner task (the one that runs
// the kkproto Endpoint). The NimBLE callbacks run in the NimBLE host task and talk to the owner through
// a queue and atomics.
//
// Typical owner loop:
//     n = recv(buf, sizeof buf, wait);   if (n > 0) endpoint.on_frame(buf, n, now);
//     if (epoch() != seen) { seen = epoch(); endpoint.reset(); if (connected()) { start session } }
// =============================================================================

namespace blelink {

enum class Role : uint8_t { Peripheral, Central };

/// A Bluetooth device address (layout of NimBLE's ble_addr_t: type 0 = public, 1 = random).
struct Addr {
    uint8_t type = 0;
    uint8_t val[6] = {};
};

/// Starts the BLE stack (once). The caller has already initialised NVS. Peripheral: starts advertising at
/// once. Central: scans only after one of the scan_*() commands. Returns false if the stack cannot start
/// (or the build has no NimBLE).
bool start(Role role, const char* device_name);
bool started();

// ---- data path
/// Sends one message. False if there is no link or the message could not be queued.
bool send(const uint8_t* msg, size_t n);
/// Waits up to `wait` for a complete message. Returns its length (> 0) or 0 if none came.
int recv(uint8_t* buf, size_t cap, TickType_t wait);
/// A link is up and usable for messages (peripheral: the vault subscribed; central: subscribed to TX).
bool connected();
/// Changes every time a link comes up or goes down. When it differs from the value seen last time, the
/// session above is dead: reset it, and if connected() start a new one.
uint32_t epoch();
/// Drops the current connection (central: it reconnects by itself while a scan mode is set).
void disconnect();

// ---- dongle (peripheral)
/// Advertise "pairing window open" (the vault in pairing mode looks for exactly that). Takes effect at
/// once while not connected, otherwise with the next advertising.
void set_pairing_open(bool open);

// ---- vault (central); each also (re)starts scanning and keeps reconnecting until stop()
/// Look for a dongle that advertises an open pairing window and connect to it.
void scan_pairing();
/// Connect to any KeyKeeper dongle in range (used once for a dongle paired before addresses were stored;
/// the Noise handshake rejects a wrong one).
void scan_any();
/// Connect to this dongle only.
void scan_addr(const Addr& addr);
/// No scanning, no connection.
void stop();
/// Address of the current connection (central), false if not connected.
bool peer_addr(Addr* out);

} // namespace blelink
