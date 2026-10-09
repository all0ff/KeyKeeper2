#pragma once

#include "status_model.hpp"

namespace dongle::ui {

/// Needs display::init() and lvgl_port::init() to have succeeded.
bool init();

/// Redraws only what changed.
void show(const Screen& screen);

} // namespace dongle::ui
