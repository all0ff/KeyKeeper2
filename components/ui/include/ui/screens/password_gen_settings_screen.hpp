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
//
// "Generate & Type" row: a standalone way to generate a password and
// type it via USB HID WITHOUT creating or editing a vault entry at
// all -- e.g. a fresh password needed right now for some signup form
// already open elsewhere. Uses the CURRENT in-memory length_/
// uppercase_/etc, not necessarily what's been Saved yet -- lets you
// try a setting out before committing to it. Separate from
// ui::screens::AccountEditScreen's own OkLong-on-Password-row
// generate action, which always uses the SAVED settings and fills a
// specific entry's field.
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
        GenerateAndType,
        Save,
    };
    static constexpr size_t ROW_COUNT = 7;

    enum class Mode : uint8_t
    {
        Browse,
        Adjust,
    };

    void build_rows(lv_obj_t* parent);
    void render_rows();
    void move_selection(int32_t delta);
    void adjust_value(int32_t delta);

    // Adjust mode is "OK confirms, BACK cancels": begin_adjust()
    // snapshots every value adjust_value() can change, end_adjust(true)
    // keeps whatever the encoder left them at, end_adjust(false) puts
    // them back exactly as they were when this row was entered. Only
    // THIS row's pending change is discarded -- earlier confirmed
    // changes on other rows stay in the working copy until the
    // screen's own Save (or leaving it, which discards them all).
    void begin_adjust();
    void end_adjust(bool commit);

    struct AdjustSnapshot
    {
        uint8_t length;
        bool uppercase;
        bool lowercase;
        bool digits;
        bool symbols;
    };
    AdjustSnapshot adjust_snapshot_{};
    void activate();
    void save();
    void generate_and_type();

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
