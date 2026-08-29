#pragma once

#include "ui/screen.hpp"

#include "lvgl.h"

#include <memory>
#include <vector>

// =============================================================================
// ui::UiManager -- ScreenManager (docs/GUI.md section 4)
//
// Owns the shared Header/Content/Footer chrome (GUI.md section 6) and
// a stack of screens. All screen creation/destruction goes through
// here -- push()/pop()/replace() -- never through a Screen directly
// manipulating LVGL's active-screen state, per GUI.md 4's "a screen
// must not create other screens itself".
//
// Screens are owned via std::unique_ptr and destroyed (C++ Destroy
// stage) once popped -- there is no screen cache/pool. For this
// device's screen count and LVGL object complexity, rebuilding a
// screen's tree on re-entry is simple and fast enough; not worth the
// complexity of a cache.
// =============================================================================

namespace ui {

class UiManager
{
public:
    /**
     * @brief Build the Header/Content/Footer chrome under lv_screen.
     *
     * lv_screen is expected to be lv_screen_active() -- the object
     * LVGL is already configured to render (see lvgl_port.cpp).
     */
    bool init(lv_obj_t* lv_screen);

    bool is_initialized() const { return initialized_; }

    /// Push a new screen on top of the stack; it becomes Active.
    /// The previous top (if any) is Hidden but stays on the stack.
    void push(std::unique_ptr<Screen> screen);

    /// Pop the active screen (Hide, then Destroy) and reactivate
    /// (Show) whatever is now on top. No-op if only one screen (or
    /// none) remains -- there is always at least one screen once
    /// init() has pushed the first one.
    void pop();

    /// Pop the active screen and push a new one in its place, without
    /// leaving a back-stack entry (e.g. Lock -> Main Menu: BACK from
    /// Main Menu shouldn't return to the Lock screen).
    void replace(std::unique_ptr<Screen> screen);

    Screen* active() const;

    /**
     * @brief Dispatch an input action to the active screen.
     *
     * Calls Screen::on_input(); if it returns false, applies default
     * navigation (currently: BackShort -> pop(), everything else
     * ignored).
     */
    void handle_input(InputAction action);

private:
    void show_active();
    void hide_active();
    void refresh_chrome();

    lv_obj_t* lv_screen_ = nullptr;
    lv_obj_t* header_ = nullptr;
    lv_obj_t* header_label_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* footer_ = nullptr;
    lv_obj_t* footer_label_ = nullptr;

    std::vector<std::unique_ptr<Screen>> stack_;
    bool initialized_ = false;
};

} // namespace ui
