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
// Two themes now (settings::Theme::Dark/Light) -- current() switches
// on settings::all().general.theme. Palette values below are a
// reasonable-looking placeholder pass, not a considered design one
// (same caveat the ORIGINAL Dark-only palette already carried) --
// revisit both once there's an actual opinion on visual identity.
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
