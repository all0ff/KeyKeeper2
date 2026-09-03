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
 * The actual destination screens for Settings and About are added in
 * subsequent UI slices. Until then, selecting those entries displays
 * a short "not implemented yet" status message. Vault now opens a
 * real screen (VaultListScreen).
 *
 * Lock is functional and calls security::lock::lock().
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
        Vault = 0,
        Settings,
        Lock,
        About,
        Count,
    };

    static constexpr uint8_t ITEM_COUNT =
        static_cast<uint8_t>(Item::Count);

    void refresh();
    void activate();
    void move_selection(int8_t delta);

    lv_obj_t* item_labels_[ITEM_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    Item selected_ = Item::Vault;
};

} // namespace ui::screens