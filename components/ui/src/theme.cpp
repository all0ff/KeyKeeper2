#include "ui/theme.hpp"

#include "settings/settings.hpp"

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

// Same placeholder-not-design-pass caveat as Dark above -- same
// accent/warning/error/success hues carried over unchanged (they
// already read fine on a light background, no reason to invent new
// ones), background/surface/text roles inverted to a light surface
// with dark text.
const Palette LIGHT_PALETTE{
    .background = lv_color_hex(0xFAFAFA),
    .surface = lv_color_hex(0xFFFFFF),
    .primary_text = lv_color_hex(0x1A1A1A),
    .secondary_text = lv_color_hex(0x616161),
    .accent = lv_color_hex(0x0288D1),
    .warning = lv_color_hex(0xF9A825),
    .error = lv_color_hex(0xD32F2F),
    .success = lv_color_hex(0x388E3C),
};

} // namespace

const Palette& current()
{
    return (settings::all().general.theme == settings::Theme::Light) ? LIGHT_PALETTE : DARK_PALETTE;
}

} // namespace ui::theme
