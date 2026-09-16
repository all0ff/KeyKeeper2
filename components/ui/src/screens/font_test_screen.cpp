#include "ui/screens/font_test_screen.hpp"

#include "display/fonts.hpp"
#include "ui/theme.hpp"

namespace ui::screens {

void FontTestScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;
    const auto& pal = theme::current();

    lv_obj_t* latin = lv_label_create(content_parent_);
    lv_label_set_text(latin, "ASCII: ABC abc 123 !?+-");
    lv_obj_set_style_text_color(latin, pal.primary_text, 0);
    lv_obj_align(latin, LV_ALIGN_TOP_LEFT, 2, 2);

    lv_obj_t* cyr = lv_label_create(content_parent);
    lv_label_set_text(cyr, "Кириллица: Привет Мир Ёё");
    lv_obj_set_style_text_font(cyr, &keykeeper_cyrillic_16, 0);
    lv_obj_set_style_text_color(cyr, pal.primary_text, 0);
    lv_obj_align(cyr, LV_ALIGN_TOP_LEFT, 2, 28);

    lv_obj_t* mixed = lv_label_create(content_parent);
    lv_label_set_text(mixed, "PIN: 123456 — Настройки");
    lv_obj_set_style_text_font(mixed, &keykeeper_cyrillic_16, 0);
    lv_obj_set_style_text_color(mixed, pal.primary_text, 0);
    lv_obj_align(mixed, LV_ALIGN_TOP_LEFT, 2, 54);

    lv_obj_t* large = lv_label_create(content_parent);
    lv_label_set_text(large, "18px: Проверка / Безопасность");
    lv_obj_set_style_text_font(large, &keykeeper_cyrillic_18, 0);
    lv_obj_set_style_text_color(large, pal.primary_text, 0);
    lv_obj_align(large, LV_ALIGN_TOP_LEFT, 2, 84);
}

bool FontTestScreen::on_input(InputAction action)
{
    return action == InputAction::BackShort;
}

} // namespace ui::screens
