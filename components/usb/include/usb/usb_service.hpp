#pragma once

#include "usb/output_sink.hpp"
#include "usb/type_engine.hpp"
#include "vault/vault.hpp"

#include <string>

namespace usb {

// =============================================================================
// usb::Service -- high-level USB HID facade
//
// Used by UI screens (QuickScreen, AccountViewScreen, etc.)
// to print account fields without knowing HID internals.
//
// All print_* methods are non-blocking: they spawn a FreeRTOS task
// so the UI remains responsive while typing proceeds.
// =============================================================================

enum class Field {
    Url,
    Login,
    Password,
    Otp,
    LoginAndPassword, // Tab-separated: login + password
};

bool init();

// Where printed text goes. nullptr (the default) is the USB cable. With a sink set (the radio dongle),
// is_connected() asks that sink and the typing goes through it; `unavailable_text` is the status shown
// when it cannot take text (e.g. "Dongle not connected"). The sink is not owned and must outlive its use.
void set_output(OutputSink* sink, const char* unavailable_text = nullptr);

// Print a raw string (non-blocking).
void type_string(const std::string& text);

// Print a specific field from a vault entry (non-blocking).
void print_field(const vault::VaultEntry& entry, Field field);

// Check whether USB cable is connected.
bool is_connected();

/// The vault's own USB cable is mounted by a host computer (independent of the dongle sink). False when the USB
/// stack is not running. Used by energy:: to tell "powered from a computer" from "on battery".
bool cable_connected();

/// Re-enumerates on the USB bus (see hid::reattach()). Call after waking from sleep.
void reattach();

// Last status message for UI feedback.
const char* last_status();

} // namespace usb
