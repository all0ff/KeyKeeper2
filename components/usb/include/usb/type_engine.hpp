#pragma once

#include <string>

namespace usb {

// =============================================================================
// TypeEngine -- high-level string-to-keystroke translator
//
// Converts a UTF-8 string into HID keyboard reports and sends them
// out over USB, one character at a time.
//
// ASCII characters type directly via keycode_map.hpp's ascii_to_hid()
// (US QWERTY layout, matches what widgets::TextEntry can actually
// produce for that range). A CYRILLIC character (this project's font
// only covers the modern Russian alphabet, 0x400-0x45F -- see
// display/fonts.hpp) types via usb::cyrillic (cyrillic_layout.hpp):
// consecutive Cyrillic characters are grouped into one RUN, the host
// is sent a layout-switch hotkey (Alt+Shift, Windows' own default)
// before the run and again after it to switch back, and each letter
// in between is typed as the physical key ЙЦУКЕН maps it to. This is
// BEST-EFFORT, not guaranteed -- see cyrillic_layout.hpp's own file
// comment for exactly what it depends on and what happens when that
// doesn't hold (wrong characters typed, not just missing ones).
//
// Timing:
//   press_ms    -- how long a key is held down (default 10 ms)
//   inter_ms    -- delay between consecutive keys (default 10 ms)
//   chunk_ms    -- extra pause after every 32 chars (default 0)
//
// All timing parameters are configurable via settings.
// =============================================================================

class TypeEngine {
public:
    struct Timing {
        uint32_t press_ms;   // Key hold time
        uint32_t inter_ms;   // Delay between keys
        uint32_t chunk_ms;    // Pause after 32 characters

        Timing()
        : press_ms(10),
          inter_ms(10),
          chunk_ms(0)
        {
        }
    };

    // Send a complete string. Blocking call.
    // Returns number of characters successfully sent.
    size_t type_string(const std::string& text, const Timing& timing = Timing());

    // Send a single character. Blocking call.
    // Returns true on success.
    bool type_char(char c, const Timing& timing = Timing());

    // Check whether USB is connected before typing.
    bool can_type() const;

    // Last error message (for UI feedback).
    const char* last_error() const;

private:
    /// Sends Alt+Shift (bare modifiers, no regular key) -- the
    /// classic Windows layout-switch hotkey. See
    /// usb::cyrillic_layout.hpp's file comment for why this is what's
    /// sent and its limits.
    bool switch_layout(const Timing& timing);

    /// Types one CYRILLIC code point via its ЙЦУКЕН physical-key
    /// equivalent, run through the ordinary ascii_to_hid() table --
    /// does NOT itself switch layout (the caller wraps a whole RUN of
    /// these with one switch_layout() before and after, not one per
    /// character).
    bool type_cyrillic_char(uint32_t codepoint, const Timing& timing);

    const char* last_error_ = "";
};

} // namespace usb
