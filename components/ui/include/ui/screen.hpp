#pragma once

#include "lvgl.h"

// =============================================================================
// ui::Screen
//
// Base class for every screen, matching docs/GUI.md section 4's
// lifecycle exactly: Create (C++ constructor) -> Initialize
// (initialize()) -> Show (on_show()) -> Active (on_input() is called
// while a screen holds this state) -> Hide (on_hide()) -> Destroy
// (C++ destructor, via UiManager's std::unique_ptr).
//
// "Экран не должен самостоятельно создавать другие экраны" (GUI.md
// 4): a Screen must not itself build/insert another Screen's LVGL
// tree or call lv_scr_load()-style APIs. It may REQUEST a
// transition via manager() (push/pop/replace) -- UiManager still owns
// all creation/destruction mechanics. This is how e.g. the vault list
// screen gets to "selecting an entry opens the account screen"
// without owning that screen itself.
// =============================================================================

namespace ui {

class UiManager;

enum class InputAction : uint8_t
{
    RotateLeft,
    RotateRight,
    OkShort,
    OkLong,
    BackShort,
    BackLong,
};

class Screen
{
public:
    virtual ~Screen() = default;

    /// Header title text, per docs/GUI.md section 6 ("Header").
    virtual const char* title() const = 0;

    /// Footer hint text, per docs/GUI.md section 6 ("Footer"), e.g.
    /// "OK  Select    BACK  Return". Empty by default.
    virtual const char* footer_hint() const { return ""; }

    /**
     * @brief Build this screen's LVGL object tree under content_parent.
     *
     * content_parent is UiManager's shared Content area (see
     * ui_manager.hpp) -- Screen does not create its own Header/Footer,
     * those are shared chrome owned by UiManager, per GUI.md 6's
     * uniform Header/Content/Footer structure. Implementations must
     * assign root_ to the top-level object they create here (usually
     * content_parent itself, or a single child of it).
     */
    virtual void initialize(lv_obj_t* content_parent) = 0;

    /// Called right after this screen becomes the visible/active one.
    virtual void on_show() {}

    /// Called right before this screen is hidden (popped, or another
    /// screen pushed over it). The LVGL tree is destroyed after this
    /// returns -- don't keep using root() past it.
    virtual void on_hide() {}

    /**
     * @brief Handle an input action while this screen is Active.
     *
     * @return true if handled (UiManager does nothing further). false
     *         to fall back to default navigation -- currently just
     *         BackShort -> UiManager::pop().
     */
    virtual bool on_input(InputAction action)
    {
        (void)action;
        return false;
    }

    lv_obj_t* root() const { return root_; }

protected:
    friend class UiManager;

    lv_obj_t* root_ = nullptr;
    UiManager* manager_ = nullptr;

    UiManager& manager() { return *manager_; }
};

} // namespace ui
