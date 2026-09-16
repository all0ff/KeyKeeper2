#include "usb/cyrillic_layout.hpp"

namespace usb::cyrillic {

namespace {

// Index i corresponds to Cyrillic lowercase codepoint (0x430 + i),
// i.e. index 0 = а (U+0430) through index 31 = я (U+044F), in
// standard Cyrillic alphabetical order. Value is the ASCII character
// occupying the SAME PHYSICAL KEY under the standard Russian ЙЦУКЕН
// layout -- cross-checked against two independent published ЙЦУКЕН
// reference tables before writing this out, given how easy a single
// transposed letter would be to miss and how confusing the result
// would be if one slipped through (a Cyrillic letter silently typing
// as the WRONG Cyrillic letter, not an obviously-broken symbol).
//
//  а б в г д е ж з и й к л м н о п р с т у ф х ц ч ш щ ъ ы ь э ю я
constexpr char LOWER_TO_PHYSICAL[32] = {
    'f', ',', 'd', 'u', 'l', 't', ';', 'p', 'b', 'q', 'r', 'k', 'v', 'y', 'j', 'g',
    'h', 'c', 'n', 'e', 'a', '[', 'w', 'x', 'i', 'o', ']', 's', 'm', '\'', '.', 'z',
};

constexpr uint32_t LOWER_FIRST = 0x0430; // а
constexpr uint32_t LOWER_LAST = 0x044F;  // я
constexpr uint32_t UPPER_FIRST = 0x0410; // А
constexpr uint32_t UPPER_LAST = 0x042F;  // Я
constexpr uint32_t YO_LOWER = 0x0451;    // ё -- not adjacent to the others, handled separately
constexpr uint32_t YO_UPPER = 0x0401;    // Ё
constexpr char YO_PHYSICAL_KEY = '`';    // backtick -- top-left corner, same on every physical keyboard

} // namespace

bool is_cyrillic(uint32_t codepoint)
{
    return (codepoint >= LOWER_FIRST && codepoint <= LOWER_LAST) ||
           (codepoint >= UPPER_FIRST && codepoint <= UPPER_LAST) || codepoint == YO_LOWER ||
           codepoint == YO_UPPER;
}

void cyrillic_physical_key(uint32_t codepoint, char& out_physical_key, bool& out_uppercase)
{
    out_physical_key = '\0';
    out_uppercase = false;

    if (codepoint == YO_LOWER) {
        out_physical_key = YO_PHYSICAL_KEY;
        return;
    }
    if (codepoint == YO_UPPER) {
        out_physical_key = YO_PHYSICAL_KEY;
        out_uppercase = true;
        return;
    }

    if (codepoint >= LOWER_FIRST && codepoint <= LOWER_LAST) {
        out_physical_key = LOWER_TO_PHYSICAL[codepoint - LOWER_FIRST];
        return;
    }

    if (codepoint >= UPPER_FIRST && codepoint <= UPPER_LAST) {
        // Uppercase codepoints are lowercase - 0x20, same offset ASCII
        // itself uses -- so the SAME table applies, just via the
        // lowercase equivalent, plus the Shift flag.
        out_physical_key = LOWER_TO_PHYSICAL[(codepoint + 0x20) - LOWER_FIRST];
        out_uppercase = true;
        return;
    }

    // Not a recognized Cyrillic letter -- out_physical_key stays '\0'.
}

} // namespace usb::cyrillic
