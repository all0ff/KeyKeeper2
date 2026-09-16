#pragma once

#include "ui/screen.hpp"

#include <cstdint>

namespace ui::screens {

/**
 * @brief Main application menu shown after successful PIN unlock.
 *
 * This is the first screen of the authenticated application area.
 *
 * Navigation:
 *
 *   RotateLeft / RotateRight -> select item
 *   OkShort                  -> activate item
 *   BackShort                -> return to QuickScreen
 *   BackLong                 -> lock device
 *
 * Accounts, Settings, Backup, and About all open real screens. Lock
 * is functional and calls security::lock::lock().
 *
 * "Accounts" (was "Vault") pushes ui::screens::AccountsScreen -- a
 * hub for All/Favorites/Categories/Search, rather than opening
 * VaultListScreen directly. Restructured this way at the project
 * owner's request; see AccountsScreen's own header comment.
 *
 * "Font Test" is a temporary diagnostic entry used to validate
 * per-label Cyrillic fonts without changing LVGL's global theme.
 */
class MainMenu : public Screen
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
        Accounts = 0,
        Settings,
        Backup,
        Lock,
        About,
        FontTest,
        Count,
    };

    static constexpr uint8_t ITEM_COUNT =
        static_cast<uint8_t>(Item::Count);

    void refresh();
    void activate();
    void move_selection(int8_t delta);

    lv_obj_t* item_labels_[ITEM_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    Item selected_ = Item::Accounts;
};

} // namespace ui::screens
