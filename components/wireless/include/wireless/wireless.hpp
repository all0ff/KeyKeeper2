#pragma once

#include <cstdint>

// =============================================================================
// wireless -- typing through the radio dongle (the vault's side of the link)
//
// The vault talks to its own USB dongle over an encrypted link (kkproto: Noise XX pairing, Noise IK
// sessions). Today the transport is a UART cable between the two boards (GPIO10 TX / GPIO11 RX, common
// GND, 460800 baud); BLE comes later behind the same API.
//
// What this component owns:
//   - this device's long-term link key and the one paired dongle (NVS namespace "kklink");
//   - the "wireless typing" switch (also NVS, default OFF);
//   - one task that runs the link: UART, the kkproto Endpoint, reconnecting, pairing;
//   - the usb::DongleChannel / usb::DongleSink that carry the typed text over that link, wired into
//     usb::set_output() while the switch is on.
//
// While wireless typing is ON, every "Print ..." action goes to the dongle (and fails with "Dongle not
// connected" when it is not linked); the vault's own USB cable is not used. While it is OFF the UART is not
// even initialised and the pins stay free.
//
// All calls are thread-safe. Without CONFIG_KEYKEEPER_KKPROTO the component is a stub: supported() is
// false and everything else does nothing.
// =============================================================================

namespace wireless {

enum class Phase : uint8_t {
    Unsupported, ///< built without the protocol library
    Off,         ///< wireless typing is switched off
    NotPaired,   ///< on, but no dongle is paired
    Connecting,  ///< on and paired: looking for the dongle
    Linked,      ///< the encrypted session works
    Pairing,     ///< pairing: looking for a dongle with its pairing window open
    Confirming,  ///< pairing: compare the code with the dongle's screen and confirm
};

/// What happened to the last pairing (shown once on the pairing screen).
enum class Outcome : uint8_t { None, Paired, Rejected, Failed };

struct Status {
    Phase phase = Phase::Off;
    bool enabled = false;
    bool paired = false;
    bool dongle_usb_ready = false; ///< Linked, and the dongle reports a PC behind its USB port
    bool local_confirmed = false;  ///< Confirming: this side already said yes
    char code[8] = {};             ///< "123456" while Confirming, else ""
    char peer[17] = {};            ///< first 8 bytes of the paired dongle's key in hex, "" if none
    Outcome outcome = Outcome::None;
};

/// Loads the key / settings and starts the link task. Call once at boot, after storage (NVS) is up.
/// Returns false only for a real failure (no key could be made); an unsupported build returns true.
bool init();
bool supported();

bool enabled();
/// Switches wireless typing on or off (persisted). On: the link task brings the UART up and, if a dongle is
/// paired, connects. Off: says goodbye, releases the UART and stops routing print actions to the dongle.
bool set_enabled(bool on);

Status status();

/// Starts a pairing with a dongle whose pairing window is open. Switches wireless typing on. The user must
/// confirm() the code shown by status().code; the dongle's user confirms theirs.
bool start_pairing();
/// The user compared the codes: yes (true) or no (false).
void confirm(bool accept);
/// Gives up a pairing in progress.
void cancel_pairing();
/// Forgets the paired dongle (the dongle itself is not told; it keeps its side until forgotten there).
bool forget();
/// Clears Status::outcome after the UI has shown it.
void clear_outcome();

} // namespace wireless
