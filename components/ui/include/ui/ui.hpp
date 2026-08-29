#pragma once

// =============================================================================
// ui
//
// Public entry point. Owns the UiManager instance and a background
// task that drains input::poll_event(), translates each event into a
// Screen-level InputAction (see screen.hpp), and dispatches it to the
// active screen under the lvgl_port lock.
//
// Pushes QuickScreen (docs/GUI.md section 7, the "Main Screen") as
// the first screen -- it is always the home/idle screen and reflects
// the current lock state itself; ui::init() does not branch on
// security::lock::state() to decide a different starting screen.
// =============================================================================

namespace ui {

/**
 * @brief Initialize the UI: build the shared chrome, push the initial
 *        screen, and start the input-dispatch task.
 *
 * Must be called after lvgl_port::init(), input::init(), and
 * security::init() (QuickScreen reads lock state immediately).
 */
bool init();

bool is_initialized();

} // namespace ui
