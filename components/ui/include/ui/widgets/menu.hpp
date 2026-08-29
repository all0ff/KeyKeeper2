#pragma once

#include "ui/icon.hpp"
#include "ui/screen.hpp"

#include "lvgl.h"

#include <cstddef>

// =============================================================================
// ui::widgets::MenuList
//
// Reusable vertical, encoder-navigable list of labeled items -- the
// pattern docs/GUI.md needs repeatedly (Main Menu section 9, and
// later Vault/Settings lists). Rotate moves the selection (scrolling
// it into view), OkShort activates it. The widget has no idea what
// each item DOES -- it just reports which index was activated; the
// owning screen decides what that means.
// =============================================================================

namespace ui::widgets {

struct MenuItem
{
    const char* label;
    icon::IconId icon;
};

enum class MenuResult : uint8_t
{
    Ignored,   ///< Action not handled here (e.g. BackShort) -- owning screen decides.
    Moved,     ///< Selection changed.
    Activated, ///< OkShort on the current selection -- see selected_index().
};

class MenuList
{
public:
    /// items must outlive this MenuList (a static/const array owned by
    /// the caller is expected -- no copy is made).
    void init(lv_obj_t* parent, const MenuItem* items, size_t count);

    MenuResult on_input(InputAction action);

    size_t selected_index() const { return selected_; }

private:
    void render();

    lv_obj_t* container_ = nullptr;

    const MenuItem* items_ = nullptr;
    size_t count_ = 0;
    size_t selected_ = 0;

    static constexpr size_t MAX_ITEMS = 12;
    lv_obj_t* rows_[MAX_ITEMS]{};
    lv_obj_t* row_labels_[MAX_ITEMS]{};
};

} // namespace ui::widgets
