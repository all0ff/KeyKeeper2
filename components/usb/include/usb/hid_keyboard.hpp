#pragma once

#include <cstdint>
#include <string>

namespace usb {

// =============================================================================
// hid::Keyboard -- TinyUSB HID Keyboard interface
//
// Wraps tud_hid_keyboard_report() with proper press/release timing.
// All methods are blocking (vTaskDelay) -- call from a dedicated task
// or accept the brief pauses.
// =============================================================================

namespace hid {

// Initialise TinyUSB HID device. Called once at boot.
bool init();

// Check whether USB cable is connected and host enumerated us.
bool is_connected();

// Send a single key press (modifier + keycode) and hold for |press_ms|.
// Automatically releases all keys afterwards.
// Returns false if not connected.
bool send_key(uint8_t keycode, uint8_t modifier, uint32_t press_ms = 10);

// Release all keys (send empty report).
void release_all();

// Get last error string (for UI status messages).
const char* last_error();

} // namespace hid

} // namespace usb
