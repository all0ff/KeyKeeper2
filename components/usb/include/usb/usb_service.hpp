#pragma once

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

// Print a raw string (non-blocking).
void type_string(const std::string& text);

// Print a specific field from a vault entry (non-blocking).
void print_field(const vault::VaultEntry& entry, Field field);

// Check whether USB cable is connected.
bool is_connected();

// Last status message for UI feedback.
const char* last_status();

} // namespace usb
