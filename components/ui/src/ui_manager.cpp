#include "ui/ui_manager.hpp"

#include "ui/theme.hpp"

#include "display/display.hpp"

#include "esp_log.h"

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
    lv_obj_center(header_label_);

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

    initialized_ = true;
    return true;
}

void UiManager::push(std::unique_ptr<Screen> screen)
{
    if (!initialized_) {
        return;
    }

    hide_active();

    // Each screen gets its own child container under the shared
    // content_ area, sized to fill it. This is what makes Hide
    // ("become invisible, keep the LVGL tree") and Destroy ("actually
    // delete it") two distinct steps, per docs/GUI.md 4's lifecycle,
    // instead of tearing down and rebuilding on every navigation.
    lv_obj_t* container = lv_obj_create(content_);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));

    screen->manager_ = this;
    screen->root_ = container;
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
        lv_obj_del(leaving->root()); // Destroy stage: LVGL tree gone
    }
    stack_.pop_back(); // Destroy stage: C++ object gone

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
    screen->initialize(container);

    stack_.push_back(std::move(screen));
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

    // Default navigation for anything a screen didn't handle itself.
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
