#pragma once

#include "ui/screen.hpp"

#include "settings/settings_types.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::GeneralSettingsScreen
//
// docs/GUI.md 14's General Settings: Language, Theme, Display
// Brightness, Screen Timeout, Auto Lock. "Auto Lock" itself lives
// under Security here -- settings::SecuritySettings owns
// auto_lock_enabled/auto_lock_timeout_s (see settings_types.hpp), not
// settings::GeneralSettings, despite GUI.md listing "Auto Lock" under
// General. Rather than fight that mismatch, the toggle is on
// SecuritySettingsScreen instead, where the data actually lives; not
// a bug, a documented grouping difference from GUI.md's section
// layout.
//
// Rotate moves the row selection; OkShort enters "adjust" mode for a
// row (rotate now changes ITS value instead), OkShort/BackShort exits
// adjust mode back to the row list. Brightness is applied live via
// display::set_brightness() as you adjust it (so you can see the
// effect immediately) and reverted to the value that was active on
// entry if you leave without saving.
//
// Only Language and Display Brightness/Screen Timeout are
// meaningfully adjustable today: Theme has exactly one value
// (Theme::Dark, see settings_types.hpp) so cycling it is a no-op,
// kept for when a second theme exists rather than removed.
// =============================================================================

namespace ui::screens {

class GeneralSettingsScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    void on_hide() override;
    bool on_input(InputAction action) override;

private:
    enum class Row : uint8_t
    {
        Language,
        Theme,
        Brightness,
        ScreenTimeout,
        Save,
    };
    static constexpr size_t ROW_COUNT = 5;

    enum class Mode : uint8_t
    {
        Browse,
        Adjust,
    };

    void render();
    void move_selection(int32_t delta);
    void adjust_value(int32_t delta);
    void activate();
    void save();

    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    size_t selected_row_ = 0;
    Mode mode_ = Mode::Browse;

    // Working copy -- only persisted via settings::set_general() on
    // Save.
    settings::Language language_ = settings::Language::English;
    settings::Theme theme_ = settings::Theme::Dark;
    uint8_t brightness_ = 0;
    uint32_t screen_timeout_s_ = 0;

    // What was actually active (persisted) when this screen opened --
    // used to revert the live brightness preview if the user leaves
    // without saving.
    uint8_t original_brightness_ = 0;
    bool saved_ = false;
};

} // namespace ui::screens
