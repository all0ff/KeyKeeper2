#include "ui/widgets/keyboard.hpp"

#include "ui/theme.hpp"

namespace ui::widgets {

void PinEntry::init(lv_obj_t* parent, const Config& cfg)
{
    cfg_ = cfg;
    if (cfg_.length > MAX_LENGTH) {
        cfg_.length = MAX_LENGTH;
    }
    cursor_ = 0;
    spin_value_ = 0;
    buffer_[0] = '\0';

    const theme::Palette& pal = theme::current();

    container_ = lv_obj_create(parent);
    lv_obj_remove_style_all(container_);
    lv_obj_set_size(container_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(container_, 6, 0);

    for (uint8_t i = 0; i < cfg_.length; ++i) {
        lv_obj_t* box = lv_obj_create(container_);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, 20, 24);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(box, pal.surface, 0);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_border_color(box, pal.secondary_text, 0);

        lv_obj_t* label = lv_label_create(box);
        lv_obj_set_style_text_color(label, pal.primary_text, 0);
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
            spin_value_ = static_cast<uint8_t>((spin_value_ + 1) % 10);
            render();
            return true;

        case InputAction::RotateLeft:
            spin_value_ = (spin_value_ == 0) ? 9 : static_cast<uint8_t>(spin_value_ - 1);
            render();
            return true;

        case InputAction::OkShort:
            if (cursor_ >= cfg_.length) {
                return true; // already complete -- ignore further OK presses
            }
            buffer_[cursor_] = static_cast<char>('0' + spin_value_);
            ++cursor_;
            buffer_[cursor_] = '\0';
            spin_value_ = 0;
            render();
            return true;

        case InputAction::BackShort:
            if (cursor_ == 0) {
                return false; // nothing to undo -- owning screen decides
            }
            --cursor_;
            buffer_[cursor_] = '\0';
            spin_value_ = 0;
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
    for (char& c : buffer_) {
        c = '\0';
    }
    render();
}

} // namespace ui::widgets
