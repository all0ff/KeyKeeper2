#pragma once

#include <cstdint>

// =============================================================================
// ButtonDetector -- turns the raw level of one active-low button into "short press" and "long press".
//
//   short: released after being held for at least debounce_ms but less than long_ms (fires on release)
//   long:  still held after long_ms (fires ONCE, while the button is still down, so the user gets the
//          feedback without having to guess when to let go; the release that follows fires nothing)
//
// Pure logic, no hardware: feed it the level every few milliseconds. Tested on a PC.
// =============================================================================

namespace dongle {

enum class Press : uint8_t { None, Short, Long };

class ButtonDetector {
public:
    explicit ButtonDetector(uint32_t long_ms = 3000, uint32_t debounce_ms = 30) : long_ms_(long_ms), debounce_ms_(debounce_ms) {}

    /// level_low: true while the button is pressed. now_ms: any monotonic millisecond clock (wrap-safe).
    Press feed(bool level_low, uint32_t now_ms)
    {
        if (level_low) {
            if (!down_) {
                down_ = true;
                long_sent_ = false;
                down_at_ = now_ms;
                return Press::None;
            }
            if (!long_sent_ && static_cast<uint32_t>(now_ms - down_at_) >= long_ms_) {
                long_sent_ = true;
                return Press::Long;
            }
            return Press::None;
        }
        if (!down_) {
            return Press::None;
        }
        down_ = false;
        const uint32_t held = static_cast<uint32_t>(now_ms - down_at_);
        if (!long_sent_ && held >= debounce_ms_ && held < long_ms_) {
            return Press::Short;
        }
        return Press::None;
    }

    bool is_down() const { return down_; }
    /// For the screen: how long the button has been held (0 when up).
    uint32_t held_ms(uint32_t now_ms) const { return down_ ? static_cast<uint32_t>(now_ms - down_at_) : 0; }

private:
    uint32_t long_ms_;
    uint32_t debounce_ms_;
    bool down_ = false;
    bool long_sent_ = false;
    uint32_t down_at_ = 0;
};

} // namespace dongle
