#pragma once

#include "lvgl.h"

// =============================================================================
// display::fonts -- custom LVGL fonts with Cyrillic + Latin coverage.
//
// LVGL's built-in Montserrat fonts (CONFIG_LV_FONT_MONTSERRAT_16/18,
// the only ones this project enables) cover Basic Latin only -- no
// Cyrillic glyphs at all. These two fonts were generated from PT Sans
// (SIL Open Font License, ParaType -- a font specifically designed
// for high-quality Cyrillic+Latin coverage, chosen for that reason
// over Montserrat's own source, which doesn't have a Cyrillic cut)
// via LVGL's own official lv_font_conv tool:
//
//   lv_font_conv --font PT_Sans-Web-Regular.ttf --size <16|18> --bpp 4
//     --format lvgl -r 0x20-0x7E,0x400-0x45F,0x2013,0x2014,0x2022,0x2026,0x2116
//     --lv-font-name keykeeper_cyrillic_<16|18>
//
// Range covers printable ASCII (0x20-0x7E, matching
// widgets::TextEntry's existing ALPHABET) + the modern Russian
// alphabet including Ё/ё (0x400-0x45F) + common typography symbols
// that plain ASCII doesn't have: en dash U+2013, em dash U+2014,
// bullet U+2022 (also used as the password-mask glyph elsewhere in
// the UI), ellipsis U+2026, and the Russian/Cyrillic-locale number
// sign № U+2116. Widened from an initial ASCII+Cyrillic-only version
// after ui::screens::FontTestScreen's own test text (which used an em
// dash) exposed the gap -- none of these five are typeable via
// widgets::TextEntry's character wheel, so they only matter for
// fixed/hardcoded UI strings, not user-entered text. Deliberately NOT
// the full Cyrillic Unicode block (0x400-0x4FF also has Ukrainian/
// Belarusian/other Slavic extensions) -- kept narrow to hold the
// flash footprint down; widen the range and regenerate if broader
// script coverage is ever needed.
//
// keykeeper_cyrillic_16 is applied as the LVGL DEFAULT font for the
// whole app via lv_theme_default_init() (see lvgl_port.cpp) -- most
// labels never set an explicit font and inherit this, so this one
// change is what makes Cyrillic text render anywhere text is shown,
// without editing dozens of individual screens.
//
// keykeeper_cyrillic_18 replaces the explicit
// &lv_font_montserrat_18 widgets::PinEntry used for its digit boxes
// (see keyboard.cpp) -- PinEntry itself never needs Cyrillic, but
// reusing this family keeps the two enabled Montserrat fonts from
// needing to stay loaded at all, and keeps digit box glyphs visually
// consistent with the rest of the UI.
//
// IMPORTANT -- this only makes Cyrillic text RENDER. Actually TYPING
// Cyrillic into an account field is a separate, much larger gap:
// widgets::TextEntry's character wheel (ALPHABET in text_entry.cpp)
// is ASCII-only, and its whole buffer/cursor model assumes one byte
// equals one character, which doesn't hold for UTF-8-encoded
// Cyrillic (2 bytes per character). Not addressed here -- flagged as
// a separate, substantial follow-up.
// =============================================================================

extern const lv_font_t keykeeper_cyrillic_16;
extern const lv_font_t keykeeper_cyrillic_18;
