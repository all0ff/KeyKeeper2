#include "usb/type_engine.hpp"
#include "usb/cyrillic_layout.hpp"
#include "usb/hid_keyboard.hpp"
#include "usb/keycode_map.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace usb {

namespace {
constexpr char TAG[] = "usb.type";

// Held slightly longer than a normal character press -- OS
// layout-switch hotkey handlers are sometimes picky about very brief
// modifier-only taps being recognized at all.
constexpr uint32_t LAYOUT_SWITCH_HOLD_MS = 50;
// Give the host a moment to actually apply the new layout before the
// first character of the run goes out.
constexpr uint32_t LAYOUT_SWITCH_SETTLE_MS = 80;

/**
 * @brief Decode ONE UTF-8 code point starting at text[pos].
 *
 * @return Number of bytes consumed (1-4), or 0 if pos is already at
 *         the end. Malformed/truncated sequences degrade to
 *         consuming just the one lead byte, decoded as itself -- this
 *         project only ever produces well-formed UTF-8 (from
 *         widgets::TextEntry or vault storage, both closed loops), so
 *         this is a safety fallback, not a real code path.
 */
size_t utf8_decode(const std::string& text, size_t pos, uint32_t& out_codepoint)
{
    const size_t remaining = text.size() - pos;
    const auto b0 = static_cast<unsigned char>(text[pos]);

    size_t seq_len = 1;
    uint32_t codepoint = b0;

    if ((b0 & 0x80) == 0x00) {
        seq_len = 1;
        codepoint = b0;
    } else if ((b0 & 0xE0) == 0xC0 && remaining >= 2) {
        seq_len = 2;
        codepoint = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0 && remaining >= 3) {
        seq_len = 3;
        codepoint = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0 && remaining >= 4) {
        seq_len = 4;
        codepoint = b0 & 0x07;
    } else {
        // Lead byte doesn't look like valid UTF-8, or the sequence
        // would run past the end of the string -- consume just this
        // one byte as a fallback (see this function's own comment).
        out_codepoint = b0;
        return 1;
    }

    for (size_t i = 1; i < seq_len; ++i) {
        const auto cb = static_cast<unsigned char>(text[pos + i]);
        if ((cb & 0xC0) != 0x80) {
            // Not a valid continuation byte -- bail out to the
            // single-byte fallback rather than misdecode.
            out_codepoint = b0;
            return 1;
        }
        codepoint = (codepoint << 6) | (cb & 0x3F);
    }

    out_codepoint = codepoint;
    return seq_len;
}

} // namespace

bool TypeEngine::can_type() const
{
    return hid::is_connected();
}

const char* TypeEngine::last_error() const
{
    return last_error_;
}

bool TypeEngine::type_char(char c, const Timing& timing)
{
    if (!hid::is_connected()) {
        last_error_ = "USB not connected";
        return false;
    }

    const KeyMapping km = ascii_to_hid(c);
    if (km.keycode == keycode::NONE) {
        last_error_ = "Unsupported character";
        // Character not supported on US keyboard and isn't Cyrillic
        // either (Cyrillic is handled separately in type_string(),
        // never reaches here) -- skip silently.
        ESP_LOGW(TAG, "Unsupported character: 0x%02X", static_cast<unsigned char>(c));
        return true; // not a hard failure
    }

    const bool ok = hid::send_key(km.keycode, km.modifier, timing.press_ms);
    if (!ok) {
        last_error_ = hid::last_error();
        return false;
    }

    if (timing.inter_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(timing.inter_ms));
    }

    return true;
}

bool TypeEngine::switch_layout(const Timing& timing)
{
    (void)timing;
    const bool ok = hid::send_key(keycode::NONE, modifier::LEFT_ALT | modifier::LEFT_SHIFT, LAYOUT_SWITCH_HOLD_MS);
    if (!ok) {
        last_error_ = hid::last_error();
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(LAYOUT_SWITCH_SETTLE_MS));
    return true;
}

bool TypeEngine::type_cyrillic_char(uint32_t codepoint, const Timing& timing)
{
    if (!hid::is_connected()) {
        last_error_ = "USB not connected";
        return false;
    }

    char physical_key = '\0';
    bool uppercase = false;
    cyrillic::cyrillic_physical_key(codepoint, physical_key, uppercase);

    if (physical_key == '\0') {
        // Not actually a recognized Cyrillic letter (shouldn't reach
        // here -- type_string() only calls this for code points
        // cyrillic::is_cyrillic() already confirmed) -- skip.
        ESP_LOGW(TAG, "Unsupported Cyrillic code point: U+%04X", static_cast<unsigned>(codepoint));
        return true;
    }

    KeyMapping km = ascii_to_hid(physical_key);
    if (uppercase) {
        km.modifier |= modifier::LEFT_SHIFT;
    }

    const bool ok = hid::send_key(km.keycode, km.modifier, timing.press_ms);
    if (!ok) {
        last_error_ = hid::last_error();
        return false;
    }

    if (timing.inter_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(timing.inter_ms));
    }

    return true;
}

size_t TypeEngine::type_string(const std::string& text, const Timing& timing)
{
    if (!hid::is_connected()) {
        last_error_ = "USB not connected";
        ESP_LOGW(TAG, "type_string: USB not connected");
        return 0;
    }

    size_t sent = 0;
    size_t pos = 0;
    size_t char_index = 0; // for chunk_ms pacing -- counts CHARACTERS, not bytes

    while (pos < text.size()) {
        uint32_t codepoint = 0;
        const size_t consumed = utf8_decode(text, pos, codepoint);
        if (consumed == 0) {
            break; // shouldn't happen (pos < text.size() guarantees at least 1 byte), safety only
        }

        bool ok;
        if (cyrillic::is_cyrillic(codepoint)) {
            // Group the WHOLE run of consecutive Cyrillic characters
            // under one layout switch, not one switch per letter --
            // see cyrillic_layout.hpp's file comment for why (this is
            // already a best-effort feature; switching once per run
            // is both faster and less likely to confuse the host than
            // switching back and forth for every single letter).
            if (!switch_layout(timing)) {
                break;
            }

            ok = true;
            while (pos < text.size()) {
                uint32_t run_codepoint = 0;
                const size_t run_consumed = utf8_decode(text, pos, run_codepoint);
                if (run_consumed == 0 || !cyrillic::is_cyrillic(run_codepoint)) {
                    break;
                }
                if (!type_cyrillic_char(run_codepoint, timing)) {
                    ok = false;
                    break;
                }
                pos += run_consumed;
                ++sent;
                ++char_index;
                if (timing.chunk_ms > 0 && char_index % 32 == 0) {
                    vTaskDelay(pdMS_TO_TICKS(timing.chunk_ms));
                }
            }

            switch_layout(timing); // switch back, regardless of whether the run above fully succeeded
            if (!ok) {
                break;
            }
            continue; // pos already advanced past the whole run
        }

        // Non-Cyrillic: only single-byte (ASCII) code points are
        // otherwise expected here (this project's font/TextEntry
        // don't produce anything else) -- type_char() takes the raw
        // byte directly.
        ok = type_char(text[pos], timing);
        if (!ok) {
            break;
        }
        pos += consumed;
        ++sent;
        ++char_index;

        if (timing.chunk_ms > 0 && char_index % 32 == 0) {
            vTaskDelay(pdMS_TO_TICKS(timing.chunk_ms));
        }
    }

    ESP_LOGI(TAG, "Typed %zu characters", sent);
    return sent;
}

} // namespace usb
