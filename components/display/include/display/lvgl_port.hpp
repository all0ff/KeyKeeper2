#pragma once

// =============================================================================
// lvgl_port
//
// Integrates LVGL 9 with the display component: creates the LVGL display
// object, owns the draw buffers, drives lv_tick_inc()/lv_timer_handler()
// from a dedicated FreeRTOS task, and provides the locking primitive
// every other task must use before calling any lv_* function.
//
// LVGL itself is not thread-safe. Any component (e.g. gui) that touches
// lv_ objects from a task other than the internal LVGL task MUST wrap
// that access with lvgl_port::lock() / lvgl_port::unlock().
// =============================================================================

namespace lvgl_port {

/**
 * @brief Initialize LVGL and start the LVGL task.
 *
 * Must be called after display::init() has succeeded. Safe to call
 * once; a second call is a no-op that returns true.
 *
 * @return true on success, false if initialization failed.
 */
bool init();

/**
 * @brief Return whether lvgl_port::init() has completed successfully.
 */
bool is_initialized();

/**
 * @brief Acquire the LVGL lock.
 *
 * Recursive: safe to call again from the same task while already held.
 * Blocks until available.
 */
void lock();

/**
 * @brief Release the LVGL lock previously acquired with lock().
 */
void unlock();

/**
 * @brief Flip the screen 180 degrees at the PANEL level (ST7789's own
 *        MADCTL register, via LovyanGFX's setRotation() on
 *        display::internal::lcd()) -- NOT LVGL's own software
 *        rotation (lv_display_set_rotation()), which this function
 *        used at first and caused a confirmed blank/dark-display
 *        regression on real hardware, suspected to be this display's
 *        partial (40-line) LVGL draw buffer being incompatible with
 *        the full-frame buffer LVGL's software rotation typically
 *        needs.
 *
 * Panel-level rotation sidesteps that: flush_cb() passes LVGL's own
 * (always unrotated) logical coordinates straight through to
 * LovyanGFX's setAddrWindow(), which is what actually applies the
 * panel's CURRENT rotation when translating those coordinates to
 * physical panel memory -- LVGL itself never needs to know rotation
 * happened at all, so its partial-buffer flushing is unaffected
 * either way.
 *
 * Takes the LVGL lock itself -- safe to call from any task, including
 * imu::'s own auto-rotate polling task, not just the LVGL task.
 *
 * @param flipped true for 180 degrees, false for the normal
 *                 orientation this board's reference config was
 *                 verified against.
 */
void set_rotation(bool flipped);

} // namespace lvgl_port
