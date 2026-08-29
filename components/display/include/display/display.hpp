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

/**
 * @brief Logical (post-rotation) display configuration.
 *
 * width/height are the dimensions as drawn to by LVGL, i.e. after the
 * panel rotation is applied. For this board that is landscape 320x172,
 * even though the physical panel memory is portrait 172x320 — see
 * bsp::board::info().lcd for the native panel memory size.
 */
struct Config
{
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
};

/**
 * @brief Initialize the LCD hardware and display subsystem.
 *
 * Must be called after bsp::init() and before lvgl_port::init().
 * Safe to call once; a second call is a no-op that returns true.
 *
 * @return true on success, false if hardware initialization failed.
 */
bool init();

/**
 * @brief Return whether the display subsystem is initialized.
 */
bool is_initialized();

/**
 * @brief Return the active (logical, post-rotation) display configuration.
 */
const Config& config();

/**
 * @brief Enable or disable the LCD backlight.
 *
 * Does not change the stored brightness level; a subsequent
 * set_backlight(true) restores the previous brightness().
 */
void set_backlight(bool enabled);

/**
 * @brief Set LCD backlight brightness.
 *
 * @param percent Brightness from 0 to 100. Values above 100 are clamped.
 *                0 turns the backlight off.
 */
void set_brightness(uint8_t percent);

/**
 * @brief Return the current backlight brightness (0-100).
 */
uint8_t brightness();

} // namespace display
