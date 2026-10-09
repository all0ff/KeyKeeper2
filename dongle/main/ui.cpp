#include "ui.hpp"

#include "display/display.hpp"
#include "display/fonts.hpp"
#include "display/lvgl_port.hpp"
#include "esp_timer.h"
#include "lvgl.h"

#include <cstring>

namespace dongle::ui {

namespace {

lv_obj_t* s_title = nullptr;
lv_obj_t* s_big = nullptr;
lv_obj_t* s_status = nullptr;
lv_obj_t* s_hint = nullptr;
Screen s_last;
bool s_have_last = false;

// Backlight: on for a while after any activity, then off (the dongle sits in a USB port all day).
bool s_light_on = true;
uint32_t s_last_activity_ms = 0;
uint32_t s_timeout_ms = 0; // 0 = never off

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

lv_color_t tone_color(Tone t)
{
    switch (t) {
    case Tone::Good: return lv_color_hex(0x40D060);
    case Tone::Warn: return lv_color_hex(0xFFC040);
    case Tone::Bad: return lv_color_hex(0xFF6060);
    case Tone::Normal: break;
    }
    return lv_color_hex(0xE0E0E0);
}

} // namespace

bool init(uint32_t screen_timeout_s)
{
    s_timeout_ms = screen_timeout_s * 1000u;
    s_last_activity_ms = now_ms();
    lvgl_port::lock();
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    s_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_title, &keykeeper_cyrillic_18, 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0x8090A0), 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 6);

    s_big = lv_label_create(scr);
    lv_obj_set_style_text_font(s_big, &lv_font_montserrat_48, 0);
    lv_label_set_text(s_big, "");
    lv_obj_align(s_big, LV_ALIGN_CENTER, 0, -14);

    s_status = lv_label_create(scr);
    lv_obj_set_style_text_font(s_status, &keykeeper_cyrillic_18, 0);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status, 300);
    lv_label_set_text(s_status, "");
    lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 0);

    s_hint = lv_label_create(scr);
    lv_obj_set_style_text_font(s_hint, &keykeeper_cyrillic_16, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0x8090A0), 0);
    lv_label_set_text(s_hint, "");
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -6);
    lvgl_port::unlock();
    return s_title != nullptr && s_big != nullptr && s_status != nullptr && s_hint != nullptr;
}

void wake()
{
    s_last_activity_ms = now_ms();
    if (!s_light_on) {
        s_light_on = true;
        display::set_backlight(true);
    }
}

void tick()
{
    if (s_light_on && s_timeout_ms > 0 && static_cast<uint32_t>(now_ms() - s_last_activity_ms) >= s_timeout_ms) {
        s_light_on = false;
        display::set_backlight(false);
    }
}

void show(const Screen& screen)
{
    if (s_title == nullptr) {
        return;
    }
    if (s_have_last && std::strcmp(s_last.title, screen.title) == 0 && std::strcmp(s_last.big, screen.big) == 0 &&
        std::strcmp(s_last.status, screen.status) == 0 && std::strcmp(s_last.hint, screen.hint) == 0 &&
        s_last.tone == screen.tone) {
        return;
    }
    s_last = screen;
    s_have_last = true;
    wake(); // something changed on the screen: whoever looks at it should see it

    lvgl_port::lock();
    lv_label_set_text(s_title, screen.title);
    lv_label_set_text(s_big, screen.big);
    lv_label_set_text(s_status, screen.status);
    lv_label_set_text(s_hint, screen.hint);
    const lv_color_t c = tone_color(screen.tone);
    lv_obj_set_style_text_color(s_big, c, 0);
    lv_obj_set_style_text_color(s_status, c, 0);
    if (screen.big[0] != '\0') {
        lv_obj_align(s_big, LV_ALIGN_CENTER, 0, -18);
        lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 36);
    } else {
        lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 0);
    }
    lvgl_port::unlock();
}

} // namespace dongle::ui
