#pragma once

#include "ui/screen.hpp"

// =============================================================================
// ui::screens::SettingsScreen
//
// docs/GUI.md section 14: settings split into four independent
// sections -- General, USB, Security, System. This screen is just the
// hub list; each section is its own screen.
//
// WiFi added as a fifth section -- not in GUI.md 14's own list at
// all (that document predates the Wi-Fi/Web subsystem entirely), but
// there's nowhere better for it to live, and it follows the exact
// same "list of settings sections" pattern as the other four.
//
// Each item pushes its own screen: GeneralSettingsScreen,
// UsbSettingsScreen, SecuritySettingsScreen, SystemInfoScreen,
// WifiSettingsScreen.
// =============================================================================

namespace ui::screens {

class SettingsScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Item : uint8_t
    {
        General,
        Usb,
        Security,
        System,
        Wifi,
    };
    static constexpr size_t ITEM_COUNT = 5;

    void render();
    void move_selection(int32_t delta);
    void activate();

    lv_obj_t* item_labels_[ITEM_COUNT]{};
    lv_obj_t* status_label_ = nullptr;
    size_t selected_ = 0;
};

} // namespace ui::screens
