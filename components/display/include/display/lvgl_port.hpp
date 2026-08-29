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

} // namespace lvgl_port
