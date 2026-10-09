#pragma once

#include "status_model.hpp"

#include <cstdint>

namespace dongle::ui {

/// Needs display::init() and lvgl_port::init() to have succeeded.
/// screen_timeout_s: the backlight goes off after this many seconds without activity (0: stays on).
bool init(uint32_t screen_timeout_s);

/// Activity (a key typed, BOOT pressed...): backlight on, timeout restarts. Called on every screen change too.
void wake();

/// Call regularly: switches the backlight off after the timeout.
void tick();

/// Redraws only what changed.
void show(const Screen& screen);

} // namespace dongle::ui
