#include "ui/widgets/keyboard.hpp"

#include "ui/theme.hpp"

namespace ui::widgets {

void PinEntry::init(lv_obj_t* parent, const Config& cfg)
{
    cfg_ = cfg;

    if (cfg_.length > MAX_LENGTH) {
        cfg_.length = MAX_LENGTH;
    }

    if (cfg_.min_length > cfg_.length) {
    cfg_.min_length = cfg_.length;
    }

    cursor_ = 0;
    spin_value_ = 0;
    finished_ = false;
    buffer_[0] = '\0';

    const theme::Palette& pal = theme::current();

    container_ = lv_obj_create(parent);
    lv_obj_remove_style_all(container_);
    lv_obj_set_size(container_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(container_, 6, 0);
    lv_obj_center(container_);

    for (uint8_t i = 0; i < cfg_.length; ++i) {
        lv_obj_t* box = lv_obj_create(container_);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, 24, 28);   ///Размер ячейки под шрифт 18px (была 22*26 под 16px)
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(box, pal.surface, 0);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_border_color(box, pal.secondary_text, 0);

        lv_obj_t* label = lv_label_create(box);
        lv_obj_set_style_text_color(label, pal.primary_text, 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0); // REVERTED to the original built-in font --
                                                                       // see lvgl_port.cpp's own revert comment,
                                                                       // same regression, same rollback
        lv_obj_center(label);
        digit_labels_[i] = label;
    }

    render();
}

void PinEntry::render()
{
    const theme::Palette& pal = theme::current();

    for (uint8_t i = 0; i < cfg_.length; ++i) {
        lv_obj_t* label = digit_labels_[i];
        lv_obj_t* box = lv_obj_get_parent(label);

        if (i < cursor_) {
            // Already-confirmed digit.
            if (cfg_.mask_confirmed) {
                lv_label_set_text(label, "*");
            } else {
                char one[2] = {buffer_[i], '\0'};
                lv_label_set_text(label, one);
            }
            lv_obj_set_style_border_color(box, pal.secondary_text, 0);
        } else if (i == cursor_) {
            // Currently being spun -- always shown in the clear so
            // the user can see what they're dialing in.
            char one[2] = {static_cast<char>('0' + spin_value_), '\0'};
            lv_label_set_text(label, one);
            lv_obj_set_style_border_color(box, pal.accent, 0);
        } else {
            // Not reached yet.
            lv_label_set_text(label, "-");
            lv_obj_set_style_border_color(box, pal.secondary_text, 0);
        }
    }
}

bool PinEntry::on_input(InputAction action)
{
    switch (action) {
        case InputAction::RotateRight:
        case InputAction::RotateLeft: {
            const bool this_is_right = (action == InputAction::RotateRight);

            if (!cfg_.dial_mode) {
                spin_value_ = this_is_right ? static_cast<uint8_t>((spin_value_ + 1) % 10)
                                             : (spin_value_ == 0 ? 9 : static_cast<uint8_t>(spin_value_ - 1));
                render();
                return true;
            }

            // Dial mode -- see this widget's own header comment for
            // the full interaction model. Expected direction is a
            // pure function of cursor_ (even slots expect Right, odd
            // expect Left), not separately tracked state -- so
            // BackShort (which only changes cursor_) never needs its
            // own logic to keep it in sync; it does still need to
            // reset dial_engaged_ (see below) for the slot it reopens.
            const bool expect_right = (cursor_ % 2) == 0;
            const bool is_last_digit = (cursor_ + 1 == cfg_.length);
            // On dial_mode's last digit with reversal-confirm turned
            // off (settings::SecuritySettings::dial_last_digit_reverses
            // == false), that final slot behaves like Standard mode
            // instead: either direction just spins, an explicit
            // OkShort (handled below, unchanged either way) confirms
            // it -- a deliberate, unambiguous "I'm done" gesture
            // instead of one more direction-reversal.
            const bool free_spin_last = is_last_digit && !cfg_.dial_last_reverses;

            if (this_is_right == expect_right) {
                dial_engaged_ = true;
                spin_value_ = this_is_right ? static_cast<uint8_t>((spin_value_ + 1) % 10)
                                             : (spin_value_ == 0 ? 9 : static_cast<uint8_t>(spin_value_ - 1));
                render();
                return true;
            }

            if (free_spin_last || !dial_engaged_) {
                // Wrong direction, but either this slot never got
                // properly engaged in the first place (a free preview
                // spin -- see this widget's own header comment: not a
                // reversal, since there's no established direction to
                // reverse FROM yet) or this is the free-spin last
                // digit, which never treats either direction as a
                // reversal at all. Spins normally either way; does
                // NOT set dial_engaged_ here, so a genuine
                // expected-direction notch is still needed to engage
                // this slot for real.
                spin_value_ = this_is_right ? static_cast<uint8_t>((spin_value_ + 1) % 10)
                                             : (spin_value_ == 0 ? 9 : static_cast<uint8_t>(spin_value_ - 1));
                render();
                return true;
            }

            // Reversal: this slot was engaged (a real expected-
            // direction notch happened first), and now a notch the
            // other way confirms the CURRENT slot's spun value, same
            // as OkShort below would, AND this same notch is applied
            // as the first spin of the NEXT slot (whose expected
            // direction is exactly this_is_right, the direction that
            // just triggered this reversal) AND engages that next
            // slot immediately -- nothing wasted, matching how
            // reversing a real combination dial feels.
            buffer_[cursor_] = static_cast<char>('0' + spin_value_);
            ++cursor_;
            buffer_[cursor_] = '\0';
            dial_engaged_ = true;
            spin_value_ = this_is_right ? 1 : 9;

            if (cfg_.finish_on_short && cursor_ >= cfg_.length) {
                finished_ = true;
            }
            render();
            return true;
        }

        case InputAction::OkShort:

            if (cursor_ >= cfg_.length) {
                return true; // already complete -- ignore further OK presses
            }

            buffer_[cursor_] = static_cast<char>('0' + spin_value_);
            ++cursor_;

            buffer_[cursor_] = '\0';

            spin_value_ = 0;
            if (cfg_.finish_on_short && cursor_ >= cfg_.length) {
                finished_ = true;
            }
            render();

            return true;

        case InputAction::OkLong:

            if (cursor_ >= cfg_.min_length && cursor_ <= cfg_.length) {
            finished_ = true;
            }

            return true;   

        case InputAction::BackShort:
            if (cursor_ == 0) {
                return false; // nothing to undo -- owning screen decides
            }
            --cursor_;
            buffer_[cursor_] = '\0';
            spin_value_ = 0;
            dial_engaged_ = false; // re-opened slot starts fresh -- see this file's own header comment
            render();
            return true;

        default:
            return false;
    }
}

void PinEntry::reset()
{
    cursor_ = 0;
    spin_value_ = 0;
    finished_ = false;
    dial_engaged_ = false;
    
    for (char& c : buffer_) {
        c = '\0';
    }
    render();
}

} // namespace ui::widgets
