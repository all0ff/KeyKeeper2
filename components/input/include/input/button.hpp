#pragma once

#include <cstdint>

#include "driver/gpio.h"

// =============================================================================
// input::Button
//
// Generic debounced GPIO button with click, long-press, and optional
// auto-repeat detection. Knows nothing about which physical button it
// represents (OK, BACK, ...) or what its events mean semantically —
// that mapping happens one layer up, in input.cpp.
//
// Polled, not interrupt-driven: call poll() from the input task at a
// fixed period (a few ms). This keeps the debounce state machine
// simple and avoids ISR-safety concerns.
// =============================================================================

namespace input {

enum class ButtonEvent : uint8_t
{
    None,
    Down,      ///< Debounced press started.
    Click,     ///< Released before reaching long_press_ms (a normal, short press).
    LongPress, ///< Held past Config::long_press_ms. Emitted once.
    Repeat,    ///< Emitted repeatedly while held, after a long press.
    Up,        ///< Released after a LongPress/Repeat had already fired.
};

class Button
{
public:
    struct Config
    {
        gpio_num_t pin;

        /// True if the button reads low when pressed (pull-up wiring).
        bool active_low = true;

        uint32_t debounce_ms = 25;
        uint32_t long_press_ms = 600;

        /// Set repeat_interval_ms = 0 to disable auto-repeat entirely.
        uint32_t repeat_interval_ms = 150;
    };

    Button() = default;

    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;

    /**
     * @brief Configure the GPIO as an input with the appropriate pull.
     *
     * @return true on success.
     */
    bool init(const Config& cfg);

    bool is_initialized() const { return initialized_; }

    /**
     * @brief Advance the debounce/hold state machine.
     *
     * Must be called periodically (e.g. every 5-10 ms) from the input
     * task. At most one meaningful event is returned per call; if two
     * logical transitions happen between polls (unlikely at a sane
     * poll rate), the earlier one is dropped in favor of Down/Up
     * consistency.
     */
    ButtonEvent poll();

    /// True while the button is currently considered pressed (debounced).
    bool is_pressed() const { return pressed_; }

private:
    enum class State : uint8_t
    {
        Idle,
        Debouncing,
        Pressed,
        LongPressed,
    };

    bool raw_is_pressed() const;

    Config cfg_{};
    bool initialized_ = false;

    State state_ = State::Idle;
    bool pressed_ = false;

    uint32_t debounce_start_ms_ = 0;
    uint32_t press_start_ms_ = 0;
    uint32_t last_repeat_ms_ = 0;
};

} // namespace input
