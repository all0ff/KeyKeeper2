#include "ui/theme.hpp"

namespace ui::theme {

namespace {

// Placeholder Dark Theme values -- reasonable-looking, not a
// considered design pass. Revisit once there's an actual opinion on
// the visual identity (docs/GUI.md specifies WHICH color roles exist,
// not their exact hex values).
const Palette DARK_PALETTE{
    .background = lv_color_hex(0x121212),
    .surface = lv_color_hex(0x1E1E1E),
    .primary_text = lv_color_hex(0xF5F5F5),
    .secondary_text = lv_color_hex(0x9E9E9E),
    .accent = lv_color_hex(0x4FC3F7),
    .warning = lv_color_hex(0xFFB300),
    .error = lv_color_hex(0xE53935),
    .success = lv_color_hex(0x66BB6A),
};

} // namespace

const Palette& current()
{
    // Only Theme::Dark exists today -- see settings_types.hpp. Switch
    // on settings::all().general.theme here once a second theme is
    // actually designed.
    return DARK_PALETTE;
}

} // namespace ui::theme
