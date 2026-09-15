#pragma once

#include "ui/screen.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::PasswordGenSettingsScreen
//
// Not part of docs/GUI.md 14's original section list (predates
// components/password_gen entirely) -- same "extra settings section"
// pattern as WifiSettingsScreen. Length (4-64, adjustable) + four
// character-class toggles (uppercase/lowercase/digits/symbols), one
// row each, plus Save -- see settings::PasswordGenSettings and
// components/password_gen's own README for why toggles instead of
// KeyKeeper 1.90's free-text "allowed characters" string.
// =============================================================================

namespace ui::screens {

class PasswordGenSettingsScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Row : uint8_t
    {
        Length,
        Uppercase,
        Lowercase,
        Digits,
        Symbols,
        Save,
    };
    static constexpr size_t ROW_COUNT = 6;

    enum class Mode : uint8_t
    {
        Browse,
        Adjust,
    };

    void build_rows(lv_obj_t* parent);
    void render_rows();
    void move_selection(int32_t delta);
    void adjust_value(int32_t delta);
    void activate();
    void save();

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    size_t selected_row_ = 0;
    Mode mode_ = Mode::Browse;

    uint8_t length_ = 16;
    bool uppercase_ = true;
    bool lowercase_ = true;
    bool digits_ = true;
    bool symbols_ = true;
};

} // namespace ui::screens
