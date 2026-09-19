#pragma once

#include <cstdint>

// =============================================================================
// display
//
// Owns LCD hardware initialization and communication (panel driver, SPI
// bus, backlight). This header exposes only what other components need
// and never leaks LVGL or LovyanGFX types — those stay internal to this
// component (see src/display_panel.hpp and lvgl_port.hpp).
//
// GUI/LVGL objects must not be created by calling display::init(). Use
// lvgl_port::init() for that, after display::init() has succeeded.
// =============================================================================

namespace display {

struct Config
{
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
};

bool init();
bool is_initialized();
const Config& config();

/**
 * @brief Enable or disable the LCD backlight.
 *
 * Does not change the stored brightness level; a subsequent
 * set_backlight(true) restores the previous brightness().
 */
void set_backlight(bool enabled);

/**
 * @brief Return whether the backlight is currently enabled.
 *
 * This is the logical backlight state, not the configured brightness
 * percentage. It allows input handling to distinguish a wake action
 * from a normal user action.
 */
bool is_backlight_enabled();

void set_brightness(uint8_t percent);
uint8_t brightness();

} // namespace display