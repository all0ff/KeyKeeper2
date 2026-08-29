#pragma once

#include "lvgl.h"

// =============================================================================
// ui::theme -- ThemeService
//
// Single source of truth for colors, per docs/GUI.md section 18
// ("Все цвета определяются ThemeService"). Screens/widgets must pull
// colors from here, never hardcode an lv_color_hex() inline -- that's
// what would make a future second theme (or a user-selectable one)
// impossible without touching every screen.
//
// Only Dark Theme exists (settings::Theme has one value, Theme::Dark)
// -- current() returns it unconditionally for now. When a second
// theme is actually designed, switch on settings::all().general.theme
// here; nothing outside this file should need to change.
// =============================================================================

namespace ui::theme {

struct Palette
{
    lv_color_t background;
    lv_color_t surface;
    lv_color_t primary_text;
    lv_color_t secondary_text;
    lv_color_t accent;
    lv_color_t warning;
    lv_color_t error;
    lv_color_t success;
};

const Palette& current();

} // namespace ui::theme
