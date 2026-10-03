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
// Password Gen (sixth section) -- same reasoning, added later for
// components/password_gen's settings (length + character-class
// toggles).
//
// Each item pushes its own screen: GeneralSettingsScreen,
// UsbSettingsScreen, SecuritySettingsScreen, SystemInfoScreen,
// WifiSettingsScreen, PasswordGenSettingsScreen.
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
    // PasswordGen stays a real enumerator even in a Lite build --
    // Item is still a complete, always-valid type either way, just
    // never reached (ITEM_COUNT below clamps selected_ to a smaller
    // range) when CONFIG_KEYKEEPER_LITE excludes the menu entry and
    // the screen it would open (see settings_screen.cpp and
    // ../../../CMakeLists.txt's own matching #if/if() blocks -- all
    // three have to agree).
    enum class Item : uint8_t
    {
        General,
        Usb,
        Security,
        System,
        Wifi,
        PasswordGen,
    };
#if CONFIG_KEYKEEPER_LITE
    static constexpr size_t ITEM_COUNT = 5;
#else
    static constexpr size_t ITEM_COUNT = 6;
#endif

    void render();
    void move_selection(int32_t delta);
    void activate();

    lv_obj_t* item_labels_[ITEM_COUNT]{};
    lv_obj_t* status_label_ = nullptr;
    size_t selected_ = 0;
};

} // namespace ui::screens
