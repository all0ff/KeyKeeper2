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
// row (rotate now changes ITS value instead); OkShort CONFIRMS the new
// value and returns to the row list, BackShort CANCELS -- the row goes
// back to what it was when OK was pressed (see begin_adjust()/
// end_adjust()). Brightness is applied live via
// display::set_brightness() as you adjust it (so you can see the
// effect immediately) and reverted to the value that was active on
// entry if you leave without saving.
//
// Orientation cycles Rotate0 -> Rotate180 -> Auto -> Rotate0. Auto
// uses components/imu (this board's onboard QMI8658 accelerometer --
// see that component's own file comment for its current confidence
// level) to flip automatically; save() starts/stops
// imu::start_auto_rotate()/stop_auto_rotate() to match, and applies
// the chosen orientation immediately via lvgl_port::set_rotation()
// either way, same "see the effect right away" reasoning as
// Brightness -- not deferred until the NEXT boot.
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
    // Orientation moved after Save (Lite strips it -- see ROW_COUNT
    // below, same reasoning/pattern as SecuritySettingsScreen's own
    // PinEntryStyle/DialLastDigit reordering): stays a real
    // enumerator either way, never behind #if itself, just
    // unreachable in a Lite build since ROW_COUNT clamps
    // selected_row_ below its ordinal.
    enum class Row : uint8_t
    {
        Language,
        Theme,
        Brightness,
        ScreenTimeout,
        Save,
        Orientation,
    };
#if CONFIG_KEYKEEPER_LITE
    static constexpr size_t ROW_COUNT = 5;
#else
    static constexpr size_t ROW_COUNT = 6;
#endif

    enum class Mode : uint8_t
    {
        Browse,
        Adjust,
    };

    void render();
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
        settings::Language language;
        settings::Theme theme;
        settings::Orientation orientation;
        uint8_t brightness;
        uint32_t screen_timeout_s;
    };
    AdjustSnapshot adjust_snapshot_{};
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
    settings::Orientation orientation_ = settings::Orientation::Rotate0;
    uint8_t brightness_ = 0;
    uint32_t screen_timeout_s_ = 0;

    // What was actually active (persisted) when this screen opened --
    // used to revert the live brightness preview if the user leaves
    // without saving.
    uint8_t original_brightness_ = 0;
    bool saved_ = false;
};

} // namespace ui::screens
