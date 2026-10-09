#pragma once

#include <cstddef>
#include <cstdint>

// =============================================================================
// The dongle's USB side (only built with CONFIG_DONGLE_USB_HID, see sdkconfig.hid).
//
// One USB device with two functions:
//   - a HID keyboard: what the vault's text is typed into the PC with;
//   - a serial port (CDC): the log, because the board has ONE usb port and TinyUSB owns it, so the
//     usual "USB Serial/JTAG" console is gone. Log lines written before a terminal is attached are
//     kept (last 8 KB) and sent when one connects.
// The keyboard code is the one proven in the vault firmware (components/usb/src/hid_keyboard.cpp):
// every report waits for tud_hid_report_complete_cb() and is retried, press and release are separate
// reports, and a short gap follows each key.
// =============================================================================

namespace dongle::usbdev {

/// Starts TinyUSB (keyboard + serial port) and routes the log to the serial port.
bool init();

/// The PC has enumerated the dongle (tud_mounted()).
bool mounted();

/// Press, hold, release. false (and last_error()) if the PC is not there or does not take the report.
bool send_key(uint8_t keycode, uint8_t modifier, uint32_t hold_ms);

const char* last_error();

/// Call from the main loop: sends buffered log text to the serial port when a terminal is attached.
void pump();

} // namespace dongle::usbdev
