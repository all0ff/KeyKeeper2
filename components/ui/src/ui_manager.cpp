#include "ui/ui_manager.hpp"

#include "ui/theme.hpp"
#include "ui/localization.hpp"

#include "display/display.hpp"
#include "display/fonts.hpp"

#include "esp_log.h"
#include "wifi/wifi_service.hpp"
#include "wireless/wireless.hpp"

namespace ui {

namespace {

constexpr char TAG[] = "ui.manager";

constexpr lv_coord_t HEADER_HEIGHT = 22;
constexpr lv_coord_t FOOTER_HEIGHT = 20;

} // namespace

bool UiManager::init(lv_obj_t* lv_screen)
{
    if (initialized_) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    lv_screen_ = lv_screen;
    const theme::Palette& pal = theme::current();

    lv_obj_set_style_bg_color(lv_screen_, pal.background, 0);
    lv_obj_set_style_bg_opa(lv_screen_, LV_OPA_COVER, 0);
    // Use the project font as the inherited UI font. Unlike changing the
    // LVGL default theme, this is local to KeyKeeper's screen tree and
    // therefore does not alter LVGL's global theme initialization.
    lv_obj_set_style_text_font(lv_screen_, &keykeeper_cyrillic_16, 0);

    // -------------------------------------------------------------------
    // Header (docs/GUI.md section 6)
    // -------------------------------------------------------------------
    header_ = lv_obj_create(lv_screen_);
    lv_obj_remove_style_all(header_);
    lv_obj_set_size(header_, LV_PCT(100), HEADER_HEIGHT);
    lv_obj_align(header_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header_, pal.surface, 0);
    lv_obj_set_style_bg_opa(header_, LV_OPA_COVER, 0);

    header_label_ = lv_label_create(header_);
    lv_obj_set_style_text_color(header_label_, pal.primary_text, 0);
    lv_obj_set_style_text_font(header_label_, &keykeeper_cyrillic_16, 0);
    lv_obj_center(header_label_);

    // Status icons at the right end of the header. The built-in Montserrat font carries the LV_SYMBOL glyphs
    // (the project's Cyrillic font does not); colour carries the state, a hidden icon means "not in use".
    wifi_icon_ = lv_label_create(header_);
    lv_obj_set_style_text_font(wifi_icon_, &lv_font_montserrat_16, 0);
    lv_label_set_text(wifi_icon_, LV_SYMBOL_WIFI);
    lv_obj_align(wifi_icon_, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_flag(wifi_icon_, LV_OBJ_FLAG_HIDDEN);

    radio_icon_ = lv_label_create(header_);
    lv_obj_set_style_text_font(radio_icon_, &lv_font_montserrat_16, 0);
    lv_label_set_text(radio_icon_, LV_SYMBOL_BLUETOOTH);
    lv_obj_align(radio_icon_, LV_ALIGN_RIGHT_MID, -28, 0);
    lv_obj_add_flag(radio_icon_, LV_OBJ_FLAG_HIDDEN);

    // -------------------------------------------------------------------
    // Footer (docs/GUI.md section 6)
    // -------------------------------------------------------------------
    footer_ = lv_obj_create(lv_screen_);
    lv_obj_remove_style_all(footer_);
    lv_obj_set_size(footer_, LV_PCT(100), FOOTER_HEIGHT);
    lv_obj_align(footer_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(footer_, pal.surface, 0);
    lv_obj_set_style_bg_opa(footer_, LV_OPA_COVER, 0);

    footer_label_ = lv_label_create(footer_);
    lv_obj_set_style_text_color(footer_label_, pal.secondary_text, 0);
    lv_obj_set_style_text_font(footer_label_, &keykeeper_cyrillic_16, 0);
    // Fixed width (not auto-sized to content, LVGL's default) is
    // required for LV_LABEL_LONG_SCROLL below to have anything to
    // detect an overflow AGAINST -- an auto-sized label would just
    // keep growing past the visible footer bar instead of ever
    // triggering the scroll, which is exactly what was happening
    // before this (confirmed on real hardware: several of this
    // project's own longer footer hints, and RU translations
    // generally running longer than their EN originals, already don't
    // fit this 320px-wide display at this font size). A hint that
    // DOES fit just sits still, same as before -- LVGL only scrolls
    // when the text is actually wider than this.
    lv_obj_set_width(footer_label_, LV_PCT(96));
    // LV_LABEL_LONG_SCROLL (not _CIRCULAR): scrolls to the end and
    // back, left-right-left -- the project owner's own preference
    // over the continuous one-direction loop _CIRCULAR does.
    lv_label_set_long_mode(footer_label_, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_text_align(footer_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(footer_label_);

    // -------------------------------------------------------------------
    // Content (docs/GUI.md section 6) -- the area screens build into.
    // Sized to fill whatever's left between header and footer.
    // -------------------------------------------------------------------
    const display::Config& disp_cfg = display::config();

    content_ = lv_obj_create(lv_screen_);
    lv_obj_remove_style_all(content_);
    lv_obj_set_size(content_, LV_PCT(100), disp_cfg.height - HEADER_HEIGHT - FOOTER_HEIGHT);
    lv_obj_align(content_, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT);
    lv_obj_set_style_bg_color(content_, pal.background, 0);
    lv_obj_set_style_bg_opa(content_, LV_OPA_COVER, 0);

    status_timer_ = lv_timer_create([](lv_timer_t* t) { static_cast<UiManager*>(lv_timer_get_user_data(t))->update_status_icons(); },
                                    1000, this);
    update_status_icons();

    initialized_ = true;
    return true;
}

void UiManager::update_status_icons()
{
    const theme::Palette& pal = theme::current();

    // Dongle: hidden while wireless typing is off or unsupported; grey = on but no link; yellow = linked, no
    // PC behind the dongle; green = linked and ready to type.
    const wireless::Status w = wireless::status();
    if (!w.enabled || w.phase == wireless::Phase::Unsupported || w.phase == wireless::Phase::Off) {
        lv_obj_add_flag(radio_icon_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_color_t c = pal.secondary_text;
        if (w.phase == wireless::Phase::Linked) {
            c = w.dongle_usb_ready ? pal.success : pal.warning;
        } else if (w.phase == wireless::Phase::Pairing || w.phase == wireless::Phase::Confirming) {
            c = pal.accent;
        }
        lv_obj_set_style_text_color(radio_icon_, c, 0);
        lv_obj_clear_flag(radio_icon_, LV_OBJ_FLAG_HIDDEN);
    }

    // Wi-Fi: hidden while off; green = connected to the network (Station); blue = own access point is up;
    // yellow = connecting; grey = lost the network; red = gave up.
    lv_color_t wc = pal.secondary_text;
    bool show = true;
    switch (wifi::state()) {
        case wifi::ConnectionState::Idle: show = false; break;
        case wifi::ConnectionState::Connecting: wc = pal.warning; break;
        case wifi::ConnectionState::Connected: wc = pal.success; break;
        case wifi::ConnectionState::Disconnected: wc = pal.secondary_text; break;
        case wifi::ConnectionState::ApRunning: wc = pal.accent; break;
        case wifi::ConnectionState::Failed: wc = pal.error; break;
    }
    if (show) {
        lv_obj_set_style_text_color(wifi_icon_, wc, 0);
        lv_obj_clear_flag(wifi_icon_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(wifi_icon_, LV_OBJ_FLAG_HIDDEN);
    }
}

void UiManager::push(std::unique_ptr<Screen> screen)
{
    if (!initialized_) {
        return;
    }

    hide_active();

    lv_obj_t* container = lv_obj_create(content_);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));

    screen->manager_ = this;
    screen->root_ = container;
    lv_obj_set_style_text_font(screen->root_, &keykeeper_cyrillic_16, 0);
    screen->initialize(container);

    stack_.push_back(std::move(screen));
    show_active();
}

void UiManager::pop()
{
    if (!initialized_ || stack_.size() <= 1) {
        return;
    }

    hide_active();

    Screen* leaving = stack_.back().get();
    if (leaving->root() != nullptr) {
        lv_obj_del(leaving->root());
    }
    stack_.pop_back();

    show_active();
}

void UiManager::replace(std::unique_ptr<Screen> screen)
{
    if (!initialized_) {
        return;
    }

    if (!stack_.empty()) {
        hide_active();
        Screen* leaving = stack_.back().get();
        if (leaving->root() != nullptr) {
            lv_obj_del(leaving->root());
        }
        stack_.pop_back();
    }

    lv_obj_t* container = lv_obj_create(content_);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));

    screen->manager_ = this;
    screen->root_ = container;
    lv_obj_set_style_text_font(screen->root_, &keykeeper_cyrillic_16, 0);
    screen->initialize(container);

    stack_.push_back(std::move(screen));
    show_active();
}

void UiManager::reset_to_root()
{
    if (!initialized_ || stack_.size() <= 1) {
        return;
    }

    hide_active();

    while (stack_.size() > 1) {
        Screen* leaving = stack_.back().get();
        if (leaving->root() != nullptr) {
            lv_obj_del(leaving->root());
        }
        stack_.pop_back();
    }

    show_active();
}

Screen* UiManager::active() const
{
    return stack_.empty() ? nullptr : stack_.back().get();
}

void UiManager::handle_input(InputAction action)
{
    Screen* top = active();
    if (top == nullptr) {
        return;
    }

    if (top->on_input(action)) {
        return;
    }

    if (action == InputAction::BackShort) {
        pop();
    }
}

void UiManager::show_active()
{
    Screen* top = active();
    if (top == nullptr) {
        return;
    }

    if (top->root() != nullptr) {
        lv_obj_clear_flag(top->root(), LV_OBJ_FLAG_HIDDEN);
    }

    refresh_chrome();
    top->on_show();
}

void UiManager::hide_active()
{
    Screen* top = active();
    if (top == nullptr) {
        return;
    }

    top->on_hide();

    if (top->root() != nullptr) {
        lv_obj_add_flag(top->root(), LV_OBJ_FLAG_HIDDEN);
    }
}

void UiManager::refresh_chrome()
{
    Screen* top = active();
    if (top == nullptr) {
        return;
    }

    lv_label_set_text(header_label_, top->title());
    lv_label_set_text(footer_label_, top->footer_hint());
}

} // namespace ui
