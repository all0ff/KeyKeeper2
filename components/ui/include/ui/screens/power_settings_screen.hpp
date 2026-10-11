#pragma once

#include "ui/screen.hpp"

#include "settings/settings_types.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::PowerSettingsScreen -- "Energy saving"
//
// Rows: Wi-Fi off after N min of no web use, dongle link (BLE) off after N min of no use, lower the CPU
// frequency at idle, battery icon style (off / icon / percent), Save. The timeouts step through a fixed list
// (off, 1, 2, 5, 10, 15, 30, 60 min). Same interaction as the other settings screens: rotate = move, OK = edit
// the row, OK = keep the value / BACK = cancel it, the Save row writes settings::PowerSettings.
// What each setting does is in components/energy/include/energy/energy.hpp.
// =============================================================================

namespace ui::screens {

class PowerSettingsScreen : public Screen
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
        WifiOff,
        BleOff,
        CpuIdle,
        Battery,
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
    void begin_adjust();
    void end_adjust(bool commit);
    void activate();
    void save();

    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    size_t selected_row_ = 0;
    Mode mode_ = Mode::Browse;

    // Working copy (persisted only by Save) and the value a row had when it was entered for editing.
    settings::PowerSettings work_{};
    settings::PowerSettings snapshot_{};
};

} // namespace ui::screens
