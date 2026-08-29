#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"

// =============================================================================
// input::Encoder
//
// Generic quadrature (EC11-style) rotary encoder driver built on the
// ESP32-S3 hardware PCNT peripheral. Knows nothing about what the
// encoder is used for (menu navigation, value adjustment, ...) — it
// only reports signed "notches" (detents) rotated since the last read.
//
// Decoding happens entirely in hardware (x4 quadrature via PCNT edge +
// level actions), so there is no ISR load on the CPU and no risk of
// missed pulses at high spin speed.
// =============================================================================

namespace input {

class Encoder
{
public:
    struct Config
    {
        gpio_num_t pin_a;
        gpio_num_t pin_b;

        /**
         * Raw PCNT edge counts per mechanical detent. Standard EC11
         * modules produce 4 quadrature edges per detent (x4 decoding).
         */
        uint8_t steps_per_notch = 2;

        /**
         * Flip rotation direction without rewiring, in case A/B are
         * swapped relative to what "clockwise = positive" expects.
         */
        bool invert = false;

        /**
         * Glitch filter applied to both PCNT channels, rejects pulses
         * shorter than this from contact bounce / electrical noise.
         */
        uint32_t glitch_filter_ns = 1000;
    };

    Encoder() = default;
    ~Encoder();

    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    /**
     * @brief Configure GPIOs and start the hardware quadrature decoder.
     *
     * @return true on success.
     */
    bool init(const Config& cfg);

    bool is_initialized() const { return unit_ != nullptr; }

    /**
     * @brief Return signed notches rotated since the last call, and
     *        reset the internal accumulator.
     *
     * Positive = clockwise, negative = counter-clockwise (before
     * Config::invert is applied at init time).
     */
    int32_t take_delta();

private:
    pcnt_unit_handle_t unit_ = nullptr;
    pcnt_channel_handle_t chan_a_ = nullptr;
    pcnt_channel_handle_t chan_b_ = nullptr;

    uint8_t steps_per_notch_ = 4;
    bool invert_ = false;

    /**
     * Raw PCNT edge counts that did not yet add up to a full notch.
     * Kept across take_delta() calls so no motion is lost when the
     * caller polls at a moment that lands mid-detent.
     */
    int32_t residual_edges_ = 0;
};

} // namespace input
