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
lv_obj_t* s_link_icon = nullptr; // corner icons (top right): radio link to the vault, USB / PC
lv_obj_t* s_usb_icon = nullptr;
Screen s_last;
bool s_have_last = false;

// Backlight: on for a while after any activity, then off (the dongle sits in a USB port all day).
bool s_light_on = true;
bool s_dimmed = false;
constexpr uint32_t DIM_BEFORE_OFF_MS = 5000; // half brightness for this long, then off (like a phone)
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

void apply_light(lv_obj_t* icon, Light l)
{
    if (icon == nullptr) {
        return;
    }
    if (l == Light::Off) {
        lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_color_t c = lv_color_hex(0x707880); // grey
    switch (l) {
    case Light::Good: c = lv_color_hex(0x40D060); break;
    case Light::Warn: c = lv_color_hex(0xFFC040); break;
    case Light::Bad: c = lv_color_hex(0xFF6060); break;
    case Light::Grey:
    case Light::Off: break;
    }
    lv_obj_set_style_text_color(icon, c, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN);
}

} // namespace

bool init(uint32_t screen_timeout_s)
{
    s_timeout_ms = screen_timeout_s * 1000u;
    s_last_activity_ms = now_ms();
    lvgl_port::lock();
    lv_obj_t* scr = lv_screen_active();
    constexpr lv_coord_t W = 320; // the labels get the full screen width and centre their text themselves: a label
                                  // that sizes itself to its text is positioned from its OLD (empty) size, and a
                                  // long text (the pairing code) then sticks out of the right edge of the screen
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    s_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_title, &keykeeper_cyrillic_18, 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0x8090A0), 0);
    lv_obj_set_width(s_title, W);
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 6);

    s_big = lv_label_create(scr);
    lv_obj_set_style_text_font(s_big, &lv_font_montserrat_48, 0);
    lv_label_set_text(s_big, "");
    lv_obj_set_width(s_big, W);
    lv_obj_set_style_text_align(s_big, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_big, LV_ALIGN_CENTER, 0, -14);

    s_status = lv_label_create(scr);
    lv_obj_set_style_text_font(s_status, &keykeeper_cyrillic_18, 0);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status, W - 20);
    lv_label_set_text(s_status, "");
    lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 0);

    s_hint = lv_label_create(scr);
    lv_obj_set_style_text_font(s_hint, &keykeeper_cyrillic_16, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0x8090A0), 0);
    lv_label_set_text(s_hint, "");
    lv_obj_set_width(s_hint, W);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -6);

    // Icons in a fixed-size row packed to the right (a hidden icon takes no room).
    lv_obj_t* row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 56, 24);
    lv_obj_align(row, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    s_link_icon = lv_label_create(row);
    lv_obj_set_style_text_font(s_link_icon, &lv_font_montserrat_18, 0);
    lv_label_set_text(s_link_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_add_flag(s_link_icon, LV_OBJ_FLAG_HIDDEN);
    s_usb_icon = lv_label_create(row);
    lv_obj_set_style_text_font(s_usb_icon, &lv_font_montserrat_18, 0);
    lv_label_set_text(s_usb_icon, LV_SYMBOL_USB);
    lv_obj_add_flag(s_usb_icon, LV_OBJ_FLAG_HIDDEN);
    lvgl_port::unlock();
    return s_title != nullptr && s_big != nullptr && s_status != nullptr && s_hint != nullptr;
}

void wake()
{
    s_last_activity_ms = now_ms();
    if (s_dimmed) {
        s_dimmed = false;
        display::set_dimmed(false);
    }
    if (!s_light_on) {
        s_light_on = true;
        display::set_backlight(true);
    }
}

void tick()
{
    if (!s_light_on || s_timeout_ms == 0) {
        return;
    }
    const uint32_t idle = static_cast<uint32_t>(now_ms() - s_last_activity_ms);
    if (idle >= s_timeout_ms + DIM_BEFORE_OFF_MS) {
        s_light_on = false;
        s_dimmed = false;
        display::set_backlight(false);
        display::set_dimmed(false);
    } else if (idle >= s_timeout_ms && !s_dimmed) {
        s_dimmed = true;
        display::set_dimmed(true);
    }
}

void show(const Screen& screen)
{
    if (s_title == nullptr) {
        return;
    }
    if (s_have_last && std::strcmp(s_last.title, screen.title) == 0 && std::strcmp(s_last.big, screen.big) == 0 &&
        std::strcmp(s_last.status, screen.status) == 0 && std::strcmp(s_last.hint, screen.hint) == 0 &&
        s_last.tone == screen.tone && s_last.link == screen.link && s_last.usb == screen.usb) {
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
    apply_light(s_link_icon, screen.link);
    apply_light(s_usb_icon, screen.usb);
    if (screen.big[0] != '\0') {
        lv_obj_align(s_big, LV_ALIGN_CENTER, 0, -18);
        lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 36);
    } else {
        lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 0);
    }
    lvgl_port::unlock();
}

} // namespace dongle::ui
