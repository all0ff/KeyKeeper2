#pragma once

#include <string>

namespace usb {

// =============================================================================
// TypeEngine -- high-level string-to-keystroke translator
//
// Converts a plain ASCII string into HID keyboard reports and sends
// them one character at a time.
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
        uint32_t press_ms = 10;   // Key hold time
        uint32_t inter_ms = 10;   // Delay between keys
        uint32_t chunk_ms = 0;    // Pause after 32 characters
    };

    // Send a complete string. Blocking call.
    // Returns number of characters successfully sent.
    size_t type_string(const std::string& text, const Timing& timing = Timing{});

    // Send a single character. Blocking call.
    // Returns true on success.
    bool type_char(char c, const Timing& timing = Timing{});

    // Check whether USB is connected before typing.
    bool can_type() const;

    // Last error message (for UI feedback).
    const char* last_error() const;

private:
    const char* last_error_ = "";
};

} // namespace usb
