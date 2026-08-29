#include "ui/widgets/text_entry.hpp"

#include "ui/theme.hpp"

#include <cstring>

namespace ui::widgets {

namespace {

// Space first (so the "empty/default" spin position is a visible
// underscore, not an invisible space), then lowercase, uppercase,
// digits, common symbols.
constexpr char ALPHABET[] =
    " abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!@#$%^&*()-_=+[]{}:;,.<>/?~`'\"\\|";

constexpr size_t ALPHABET_LEN = sizeof(ALPHABET) - 1; // exclude the null terminator

} // namespace

void TextEntry::init(lv_obj_t* parent, const Config& cfg)
{
    mask_ = cfg.mask;
    set_max_length(cfg.max_length);

    const theme::Palette& pal = theme::current();
    value_label_ = lv_label_create(parent);
    lv_obj_set_style_text_color(value_label_, pal.primary_text, 0);

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
        size_t i = 0;
        while (initial_value[i] != '\0' && i < max_length_ && i + 1 < MAX_BUFFER) {
            buffer_[i] = initial_value[i];
            ++i;
        }
        length_ = i;
        buffer_[length_] = '\0';
    }

    alphabet_index_ = 0;
    finished_ = false;
    render();
}

void TextEntry::render()
{
    char shown[MAX_BUFFER];

    if (mask_) {
        size_t i = 0;
        for (; i < length_; ++i) {
            shown[i] = '*';
        }
        shown[i] = '\0';
    } else {
        std::memcpy(shown, buffer_, length_ + 1);
    }

    const char current = ALPHABET[alphabet_index_];
    // Space renders as an underscore in the "currently spinning"
    // indicator only, so it's actually visible -- confirmed spaces in
    // the text itself render as real spaces via `shown` above.
    const char display_current = (current == ' ') ? '_' : current;

    lv_label_set_text_fmt(value_label_, "%s[%c]", shown, display_current);
}

bool TextEntry::on_input(InputAction action)
{
    switch (action) {
        case InputAction::RotateRight:
            alphabet_index_ = (alphabet_index_ + 1) % ALPHABET_LEN;
            render();
            return true;

        case InputAction::RotateLeft:
            alphabet_index_ = (alphabet_index_ == 0) ? ALPHABET_LEN - 1 : alphabet_index_ - 1;
            render();
            return true;

        case InputAction::OkShort:
            if (length_ >= max_length_ || length_ + 1 >= MAX_BUFFER) {
                return true; // at capacity -- ignore rather than overflow
            }
            buffer_[length_++] = ALPHABET[alphabet_index_];
            buffer_[length_] = '\0';
            alphabet_index_ = 0;
            render();
            return true;

        case InputAction::OkLong:
            finished_ = true;
            return true;

        case InputAction::BackShort:
            if (length_ == 0) {
                return false; // nothing to remove -- owning screen decides
            }
            --length_;
            buffer_[length_] = '\0';
            alphabet_index_ = 0;
            render();
            return true;

        default:
            return false;
    }
}

} // namespace ui::widgets
