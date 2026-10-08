#pragma once

#include "usb/output_sink.hpp"
#include "usb/typing_plan.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace usb {

// =============================================================================
// TypeEngine -- high-level string-to-keystroke translator
//
// Converts a UTF-8 string into keystrokes. Since the plan/execute split it is a
// thin facade over two pieces:
//
//   plan_events()   (typing_plan.hpp)  text -> list of HID events. Pure.
//   OutputSink      (output_sink.hpp)  plays that list; the default sink is
//                                      the USB cable.
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
// PUNCTUATION WHILE THE HOST IS ON THE RUSSIAN LAYOUT. Without the automatic
// layout switch the person puts the host on the Russian layout by hand, so every
// key this device presses -- ASCII punctuation included -- is read by that
// layout (the US "." key is the Russian "ю"). For a text that CONTAINS Cyrillic,
// ". , ? \" ; : /" are therefore typed on the keys the standard Windows Russian
// layout puts them on (see cyrillic::russian_layout_ascii()); digits and
// "! % * ( ) - _ = +" already share keys. Latin letters and @ # $ ^ & [ ] { } < >
// | ~ ' ` have no key on that layout at all: they are typed as before (and come
// out as other characters), a warning is logged, and the automatic switch is the
// only way to type them. With the automatic switch on, or for text without
// Cyrillic, nothing changes. Targets Windows, like the Alt+Shift hotkey.
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
    using Timing = TypingTiming;

    // Send a complete string. Blocking call.
    // Returns number of characters successfully sent.
    size_t type_string(const std::string& text, const Timing& timing = Timing());

    // Send a single character. Blocking call.
    // Returns true on success.
    bool type_char(char c, const Timing& timing = Timing());

    // Check whether the output is usable before typing (USB mounted).
    bool can_type() const;

    // Last error message (for UI feedback).
    const char* last_error() const;

    // Route typing somewhere other than the USB cable (not owned). nullptr
    // restores the default.
    void set_sink(OutputSink* sink) { sink_ = sink; }

private:
    OutputSink& sink() const { return sink_ != nullptr ? *sink_ : usb_cable_sink(); }

    OutputSink* sink_ = nullptr;
    const char* last_error_ = "";
};

} // namespace usb
