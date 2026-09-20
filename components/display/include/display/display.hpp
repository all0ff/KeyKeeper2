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
 * @brief Enable or disable the LCD backlight at its currently
 *        configured brightness().
 *
 * Does not change the stored brightness level; a subsequent
 * set_backlight(true) restores the previous brightness() -- including
 * restoring to 0 if that's what brightness() currently is, which is
 * exactly why this must NOT be used to track "is the screen asleep
 * for idle-timeout purposes" (see set_asleep()'s own comment for why
 * that distinction turned out to matter on real hardware).
 */
void set_backlight(bool enabled);

/**
 * @brief Return whether the backlight is currently enabled --
 *        equivalent to brightness() > 0, nothing more.
 *
 * NOT a proxy for "is the screen asleep due to idle timeout" -- see
 * set_asleep()/is_asleep() for that. An earlier version of this
 * function's own doc comment recommended using THIS for exactly that
 * purpose (input::'s wake-first-action handling did), which caused a
 * confirmed real bug: setting brightness to exactly 0 via General
 * Settings (a legitimate, intentional user choice, nothing to do with
 * idle timeout) made this function return false the same way an
 * idle-timeout sleep does, and the wake-handling code that restores
 * brightness on activity restores it to that SAME 0 -- so this stayed
 * false forever afterward, permanently treating every subsequent
 * input as a "wake-only" gesture that never reaches the UI. Kept
 * around for whatever else might care about raw backlight-on-ness,
 * but no longer used for that decision.
 */
bool is_backlight_enabled();

/**
 * @brief Mark the screen as asleep (idle-timeout) or awake --
 *        INDEPENDENT of brightness()/is_backlight_enabled(), which
 *        can legitimately be 0 while the screen is considered awake
 *        (see is_backlight_enabled()'s own comment for the real bug
 *        that conflating these two caused). This is what
 *        power_manager::Manager's own idle-timeout logic sets, and
 *        what input::'s wake-first-action handling checks instead of
 *        the old is_backlight_enabled()-based check.
 *
 * set_asleep(true) also turns the backlight fully off (same effect as
 * set_backlight(false)) as the visible part of "asleep". set_asleep(false)
 * restores it to the CURRENT brightness() (same as set_backlight(true))
 * -- if that happens to be 0 because the person genuinely set it to 0,
 * the screen stays dark, correctly, but is_asleep() itself still
 * correctly reports false, so a real subsequent input is no longer
 * swallowed by mistake.
 */
void set_asleep(bool asleep);

bool is_asleep();

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