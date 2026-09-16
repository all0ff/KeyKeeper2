#pragma once

#include <cstdint>

// =============================================================================
// usb::cyrillic -- typing Cyrillic text via a Russian keyboard-layout
// switch on the HOST, since USB HID itself has no concept of
// "Cyrillic" -- every key report is just a physical key position plus
// modifiers, and what character that produces is entirely up to
// whatever layout the HOST currently has active. See
// keycode_map.hpp's own file comment for the ASCII/US-layout side of
// this; this file is the Cyrillic side.
//
// APPROACH: ЙЦУКЕН (the standard Russian layout, what every "Russian"
// OS keyboard setting actually is) maps each Cyrillic letter to a
// SPECIFIC PHYSICAL KEY -- the same physical key that, under a US
// layout, produces a specific ASCII character. E.g. а sits where f
// is, й sits where q is. So rather than a second full HID keycode
// table, cyrillic_physical_key() maps each Cyrillic letter to the
// ASCII character occupying the SAME physical key, and the caller
// (type_engine.cpp) runs that through the EXISTING ascii_to_hid() to
// get the actual keycode -- one source of truth for physical key
// positions, not two.
//
// To actually produce Cyrillic output, type_engine.cpp additionally
// sends a layout-switch HOTKEY (Left Alt + Left Shift, held briefly
// with no other key -- see hid_keyboard.hpp's send_key(), which
// supports a bare-modifier report) before a run of Cyrillic
// characters, and again afterward to switch back. This is the
// classic Windows "switch input language" default shortcut.
//
// THIS IS BEST-EFFORT, NOT GUARANTEED, by nature -- there is no way
// for a USB HID device to know what's actually configured on the
// host it's plugged into. It depends on:
//   - The host having a Russian keyboard layout installed at all.
//   - The host's language-switch shortcut actually being Alt+Shift
//     (Windows' own long-standing default -- but user-configurable,
//     and NOT what macOS or most Linux desktops use by default).
//   - Normal Windows/typical-desktop switch-hotkey timing (a quick
//     press+release, not held).
// If any of that doesn't hold, the physical keys still get sent --
// just interpreted under whatever layout actually ends up active,
// which could type the WRONG (Latin) characters instead of nothing.
// That's a real trade-off against the OLD behavior (silently
// dropping Cyrillic characters entirely) -- flagged, not hidden.
// =============================================================================

namespace usb::cyrillic {

/**
 * @brief For a Cyrillic code point (see is_cyrillic()'s own range),
 *        returns the ASCII character occupying the SAME PHYSICAL KEY
 *        under the standard Russian ЙЦУКЕН layout, and whether the
 *        original letter was uppercase (needs an extra Shift on top
 *        of whatever ascii_to_hid() already applies for that key).
 *
 * @return '\0' in out_physical_key if codepoint isn't a recognized
 *         Cyrillic letter (this project's font only covers 0x400-0x45F,
 *         the modern Russian alphabet -- see display/fonts.hpp).
 */
void cyrillic_physical_key(uint32_t codepoint, char& out_physical_key, bool& out_uppercase);

/// True for any code point this maps a physical key for -- i.e. the
/// modern Russian alphabet, 0x410-0x44F (А-я) plus Ёё (0x401/0x451).
bool is_cyrillic(uint32_t codepoint);

} // namespace usb::cyrillic
