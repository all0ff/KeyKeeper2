#pragma once

#include <cstdint>

namespace usb {

// =============================================================================
// HID Keycode Mapping (USB HID Usage Table, section 10)
// https://usb.org/sites/default/files/hut1_21.pdf
//
// Maps ASCII characters to (hid_keycode, modifier) pairs.
// modifier bits: 0x02 = Left Shift
//
// Supports US QWERTY layout only for now.
// Russian layout support planned (Phase 6.4).
// =============================================================================

struct KeyMapping {
    uint8_t keycode;   // HID usage ID (0x04-0x38)
    uint8_t modifier;  // 0x02 if shift required, else 0
};

// Returns key mapping for a given ASCII character.
// For characters not present on a US keyboard (e.g. Cyrillic),
// returns {0, 0} and caller must handle separately.
KeyMapping ascii_to_hid(char c);

// Common HID keycodes for direct use
namespace keycode {
    constexpr uint8_t NONE      = 0x00;
    constexpr uint8_t A         = 0x04;
    constexpr uint8_t B         = 0x05;
    constexpr uint8_t ENTER     = 0x28;
    constexpr uint8_t ESCAPE    = 0x29;
    constexpr uint8_t BACKSPACE = 0x2A;
    constexpr uint8_t TAB       = 0x2B;
    constexpr uint8_t SPACE     = 0x2C;
} // namespace keycode

namespace modifier {
    constexpr uint8_t NONE       = 0x00;
    constexpr uint8_t LEFT_SHIFT = 0x02;
} // namespace modifier

} // namespace usb
