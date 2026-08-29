#include "input/button.hpp"

#include "esp_log.h"
#include "esp_timer.h"

namespace input {

namespace {

constexpr char TAG[] = "input.button";

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

} // namespace

bool Button::init(const Config& cfg)
{
    cfg_ = cfg;

    gpio_config_t io_conf{};
    io_conf.pin_bit_mask = 1ULL << cfg_.pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = cfg_.active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = cfg_.active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed for GPIO%d: %s",
                 static_cast<int>(cfg_.pin), esp_err_to_name(err));
        return false;
    }

    state_ = State::Idle;
    pressed_ = false;
    initialized_ = true;

    return true;
}

bool Button::raw_is_pressed() const
{
    const int level = gpio_get_level(cfg_.pin);
    return cfg_.active_low ? (level == 0) : (level == 1);
}

ButtonEvent Button::poll()
{
    if (!initialized_) {
        return ButtonEvent::None;
    }

    const uint32_t now = now_ms();
    const bool raw_pressed = raw_is_pressed();

    switch (state_) {
        case State::Idle:
            if (raw_pressed) {
                state_ = State::Debouncing;
                debounce_start_ms_ = now;
            }
            return ButtonEvent::None;

        case State::Debouncing:
            if (!raw_pressed) {
                // Bounced back before the debounce window elapsed.
                state_ = State::Idle;
                return ButtonEvent::None;
            }
            if (now - debounce_start_ms_ >= cfg_.debounce_ms) {
                state_ = State::Pressed;
                pressed_ = true;
                press_start_ms_ = now;
                last_repeat_ms_ = now;
                return ButtonEvent::Down;
            }
            return ButtonEvent::None;

        case State::Pressed:
            if (!raw_pressed) {
                state_ = State::Idle;
                pressed_ = false;
                return ButtonEvent::Click;
            }
            if (now - press_start_ms_ >= cfg_.long_press_ms) {
                state_ = State::LongPressed;
                last_repeat_ms_ = now;
                return ButtonEvent::LongPress;
            }
            return ButtonEvent::None;

        case State::LongPressed:
            if (!raw_pressed) {
                state_ = State::Idle;
                pressed_ = false;
                return ButtonEvent::Up;
            }
            if (cfg_.repeat_interval_ms > 0 &&
                now - last_repeat_ms_ >= cfg_.repeat_interval_ms) {
                last_repeat_ms_ = now;
                return ButtonEvent::Repeat;
            }
            return ButtonEvent::None;
    }

    return ButtonEvent::None;
}

} // namespace input
