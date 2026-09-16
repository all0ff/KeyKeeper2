#include "ui/widgets/text_entry.hpp"

#include "display/fonts.hpp"
#include "ui/theme.hpp"

#include <cstring>

namespace ui::widgets {

namespace {

// -----------------------------------------------------------------
// Character classes. Each entry is a short null-terminated UTF-8
// string (1 byte for everything Latin/digit/symbol, 2 bytes for
// Cyrillic) rather than a plain `char`, since a single char can't
// hold a Cyrillic code point in UTF-8.
//
// Space is folded into the front of LOWER (index 0), matching the
// ORIGINAL single-wheel design's own reasoning: the "empty/default"
// spin position should be a visible underscore, not something you'd
// have to hunt for -- see render()'s handling of it.
//
// Class order: LOWER -> UPPER -> DIGITS -> SYMBOLS -> CYRILLIC_LOWER
// -> CYRILLIC_UPPER -> (back to LOWER). The first four match the old
// flat alphabet's own order exactly, just now scoped one class at a
// time via BackLong instead of all run together.
// -----------------------------------------------------------------

constexpr const char* LOWER[] = {
    " ", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
};

constexpr const char* UPPER[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
    "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
};

constexpr const char* DIGITS[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
};

constexpr const char* SYMBOLS[] = {
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "-", "_", "=", "+",
    "[", "]", "{", "}", ":", ";", ",", ".", "<", ">", "/", "?", "~", "`",
    "'", "\"", "\\", "|",
};

// Modern Russian alphabet, 33 letters -- see display/fonts.hpp, this
// project's custom font only actually has glyphs for this exact
// range: 0x400-0x45F.
constexpr const char* CYRILLIC_LOWER[] = {
    "а", "б", "в", "г", "д", "е", "ё", "ж", "з", "и", "й", "к", "л", "м",
    "н", "о", "п", "р", "с", "т", "у", "ф", "х", "ц", "ч", "ш", "щ", "ъ",
    "ы", "ь", "э", "ю", "я",
};

constexpr const char* CYRILLIC_UPPER[] = {
    "А", "Б", "В", "Г", "Д", "Е", "Ё", "Ж", "З", "И", "Й", "К", "Л", "М",
    "Н", "О", "П", "Р", "С", "Т", "У", "Ф", "Х", "Ц", "Ч", "Ш", "Щ", "Ъ",
    "Ы", "Ь", "Э", "Ю", "Я",
};

struct CharClass
{
    const char* const* glyphs;
    size_t count;
};

constexpr CharClass CLASSES[] = {
    {LOWER, sizeof(LOWER) / sizeof(LOWER[0])},
    {UPPER, sizeof(UPPER) / sizeof(UPPER[0])},
    {DIGITS, sizeof(DIGITS) / sizeof(DIGITS[0])},
    {SYMBOLS, sizeof(SYMBOLS) / sizeof(SYMBOLS[0])},
    {CYRILLIC_LOWER, sizeof(CYRILLIC_LOWER) / sizeof(CYRILLIC_LOWER[0])},
    {CYRILLIC_UPPER, sizeof(CYRILLIC_UPPER) / sizeof(CYRILLIC_UPPER[0])},
};
constexpr size_t CLASS_COUNT = sizeof(CLASSES) / sizeof(CLASSES[0]);

// -----------------------------------------------------------------
// UTF-8 helpers. This widget only ever WRITES 1- or 2-byte sequences
// itself (see the classes above), but these two are written generically
// (any UTF-8 sequence length) since reset()'s initial_value could in
// principle contain any well-formed UTF-8, and being generic here is
// no more code than special-casing "1 or 2 bytes".
// -----------------------------------------------------------------

bool is_utf8_continuation_byte(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

/// Byte offset where the character ending at (but not including)
/// `pos` starts. Walks backward over continuation bytes. Returns 0 if
/// pos is already 0.
size_t utf8_prev_char_start(const char* buf, size_t pos)
{
    if (pos == 0) {
        return 0;
    }
    size_t i = pos - 1;
    while (i > 0 && is_utf8_continuation_byte(buf[i])) {
        --i;
    }
    return i;
}

/// Number of CHARACTERS (not bytes) in the first byte_len bytes of buf.
size_t utf8_char_count(const char* buf, size_t byte_len)
{
    size_t count = 0;
    for (size_t i = 0; i < byte_len; ++i) {
        if (!is_utf8_continuation_byte(buf[i])) {
            ++count;
        }
    }
    return count;
}

} // namespace

void TextEntry::init(lv_obj_t* parent, const Config& cfg)
{
    mask_ = cfg.mask;
    set_max_length(cfg.max_length);

    const theme::Palette& pal = theme::current();
    value_label_ = lv_label_create(parent);
    lv_obj_set_style_text_color(value_label_, pal.primary_text, 0);
    // keykeeper_cyrillic_16 (see display/fonts.hpp), applied
    // EXPLICITLY to this one label -- NOT via lv_theme_default_init()
    // as a global default, which caused a confirmed, serious
    // regression (blank labels app-wide). Confirmed working via
    // ui::screens::FontTestScreen using this exact same
    // set-it-on-the-specific-label approach. This widget is the
    // single most important place to have it: whatever you're
    // TYPING has to be visible, Cyrillic included.
    lv_obj_set_style_text_font(value_label_, &keykeeper_cyrillic_16, 0);

    reset(cfg.initial_value);
}

void TextEntry::set_max_length(size_t max_length)
{
    max_length_ = (max_length < MAX_BUFFER - 1) ? max_length : MAX_BUFFER - 1;
}

void TextEntry::reset(const char* initial_value)
{
    length_ = 0;
    buffer_[0] = '\0';

    if (initial_value != nullptr) {
        const size_t src_len = std::strlen(initial_value);
        size_t copy_len = src_len;
        if (copy_len > max_length_) {
            copy_len = max_length_;
        }
        if (copy_len + 1 > MAX_BUFFER) {
            copy_len = MAX_BUFFER - 1;
        }

        // Don't split a multi-byte UTF-8 character at the truncation
        // boundary -- if the byte-count cut landed mid-sequence, back
        // off to the start of that (now-incomplete) character and
        // drop it whole instead.
        if (copy_len < src_len) {
            while (copy_len > 0 && is_utf8_continuation_byte(initial_value[copy_len])) {
                --copy_len;
            }
        }

        std::memcpy(buffer_, initial_value, copy_len);
        length_ = copy_len;
        buffer_[length_] = '\0';
    }

    class_index_ = 0;
    index_in_class_ = 0;
    finished_ = false;
    render();
}

void TextEntry::advance_to_next_class()
{
    class_index_ = (class_index_ + 1) % CLASS_COUNT;
    index_in_class_ = 0;
}

void TextEntry::render()
{
    char shown[MAX_BUFFER];

    if (mask_) {
        // One '*' per CHARACTER, not per byte -- a Cyrillic letter is
        // 2 bytes but should still show as a single asterisk, the
        // same as any other one character would.
        const size_t char_count = utf8_char_count(buffer_, length_);
        size_t i = 0;
        for (; i < char_count && i < MAX_BUFFER - 1; ++i) {
            shown[i] = '*';
        }
        shown[i] = '\0';
    } else {
        std::memcpy(shown, buffer_, length_ + 1);
    }

    const char* current = CLASSES[class_index_].glyphs[index_in_class_];
    // Space renders as an underscore in the "currently spinning"
    // indicator only, so it's actually visible -- confirmed spaces in
    // the text itself render as real spaces via `shown` above.
    const char* display_current = (std::strcmp(current, " ") == 0) ? "_" : current;

    lv_label_set_text_fmt(value_label_, "%s[%s]", shown, display_current);
}

bool TextEntry::on_input(InputAction action)
{
    const CharClass& cls = CLASSES[class_index_];

    switch (action) {
        case InputAction::RotateRight:
            index_in_class_ = (index_in_class_ + 1) % cls.count;
            render();
            return true;

        case InputAction::RotateLeft:
            index_in_class_ = (index_in_class_ == 0) ? cls.count - 1 : index_in_class_ - 1;
            render();
            return true;

        case InputAction::OkShort: {
            const char* glyph = cls.glyphs[index_in_class_];
            const size_t glyph_len = std::strlen(glyph);

            // max_length_ is a BYTE budget (see the header's own
            // comment) -- checked against the glyph's actual UTF-8
            // byte length, not "1 more character", so a Cyrillic
            // letter correctly costs twice what a Latin one does
            // against the same field's storage limit.
            if (length_ + glyph_len >= MAX_BUFFER || length_ + glyph_len > max_length_) {
                return true; // at capacity -- ignore rather than overflow
            }

            std::memcpy(buffer_ + length_, glyph, glyph_len);
            length_ += glyph_len;
            buffer_[length_] = '\0';

            index_in_class_ = 0;
            render();
            return true;
        }

        case InputAction::OkLong:
            finished_ = true;
            return true;

        case InputAction::BackShort: {
            if (length_ == 0) {
                return false; // nothing to remove -- owning screen decides
            }
            length_ = utf8_prev_char_start(buffer_, length_);
            buffer_[length_] = '\0';
            index_in_class_ = 0;
            render();
            return true;
        }

        case InputAction::BackLong:
            advance_to_next_class();
            render();
            return true;

        default:
            return false;
    }
}

} // namespace ui::widgets
