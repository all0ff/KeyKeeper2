#include "ui/widgets/menu.hpp"

#include "ui/theme.hpp"

namespace ui::widgets {

void MenuList::init(lv_obj_t* parent, const MenuItem* items, size_t count)
{
    items_ = items;
    count_ = (count > MAX_ITEMS) ? MAX_ITEMS : count;
    selected_ = 0;

    const theme::Palette& pal = theme::current();

    container_ = lv_obj_create(parent);
    lv_obj_remove_style_all(container_);
    lv_obj_set_size(container_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(container_, 2, 0);
    lv_obj_set_scroll_dir(container_, LV_DIR_VER);

    for (size_t i = 0; i < count_; ++i) {
        lv_obj_t* row = lv_obj_create(container_);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 20);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 3, 0);
        lv_obj_set_style_pad_left(row, 6, 0);

        lv_obj_t* label = lv_label_create(row);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

        rows_[i] = row;
        row_labels_[i] = label;
    }

    (void)pal;
    render();
}

void MenuList::render()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < count_; ++i) {
        const bool is_selected = (i == selected_);

        lv_obj_set_style_bg_color(rows_[i], is_selected ? pal.accent : pal.background, 0);
        lv_obj_set_style_text_color(row_labels_[i], is_selected ? pal.background : pal.primary_text, 0);

        const char* symbol = icon::symbol(items_[i].icon);
        if (symbol != nullptr && symbol[0] != '\0') {
            lv_label_set_text_fmt(row_labels_[i], "%s  %s", symbol, items_[i].label);
        } else {
            lv_label_set_text(row_labels_[i], items_[i].label);
        }
    }

    if (count_ > 0) {
        lv_obj_scroll_to_view(rows_[selected_], LV_ANIM_ON);
    }
}

MenuResult MenuList::on_input(InputAction action)
{
    switch (action) {
        case InputAction::RotateRight:
            if (count_ == 0) {
                return MenuResult::Ignored;
            }
            selected_ = (selected_ + 1 < count_) ? selected_ + 1 : 0;
            render();
            return MenuResult::Moved;

        case InputAction::RotateLeft:
            if (count_ == 0) {
                return MenuResult::Ignored;
            }
            selected_ = (selected_ == 0) ? count_ - 1 : selected_ - 1;
            render();
            return MenuResult::Moved;

        case InputAction::OkShort:
            if (count_ == 0) {
                return MenuResult::Ignored;
            }
            return MenuResult::Activated;

        default:
            return MenuResult::Ignored;
    }
}

} // namespace ui::widgets
