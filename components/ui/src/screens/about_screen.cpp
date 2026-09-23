#include "ui/screens/about_screen.hpp"

#include "ui/localization.hpp"
#include "ui/theme.hpp"

#include "esp_app_desc.h"

#include <cstdio>

namespace ui::screens {

const char* AboutScreen::title() const
{
    return i18n::tr(i18n::Key::About);
}

const char* AboutScreen::footer_hint() const
{
    return i18n::tr(i18n::Key::BackReturn);
}

void AboutScreen::initialize(lv_obj_t* content_parent)
{
    const theme::Palette& pal = theme::current();

    lv_obj_t* name_label = lv_label_create(content_parent);
    lv_obj_set_style_text_color(name_label, pal.primary_text, 0);
    lv_label_set_text(name_label, "KeyKeeper2");
    lv_obj_align(name_label, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t* tagline_label = lv_label_create(content_parent);
    lv_obj_set_style_text_color(tagline_label, pal.secondary_text, 0);
    lv_label_set_text(tagline_label, i18n::tr(i18n::Key::TaglineDesc));
    lv_obj_align(tagline_label, LV_ALIGN_TOP_MID, 0, 26);

    char version_buf[48];
    const esp_app_desc_t* app_desc = esp_app_get_description();
    std::snprintf(version_buf, sizeof(version_buf), i18n::tr(i18n::Key::FirmwareVersionFmt),
                  app_desc != nullptr ? app_desc->version : i18n::tr(i18n::Key::UnknownValue));

    lv_obj_t* version_label = lv_label_create(content_parent);
    lv_obj_set_style_text_color(version_label, pal.secondary_text, 0);
    lv_label_set_text(version_label, version_buf);
    lv_obj_align(version_label, LV_ALIGN_CENTER, 0, 0);
}

bool AboutScreen::on_input(InputAction action)
{
    (void)action;
    return false; // read-only -- BackShort pops via default nav
}

} // namespace ui::screens
