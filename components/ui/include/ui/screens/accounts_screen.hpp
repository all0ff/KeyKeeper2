#pragma once

#include "ui/screen.hpp"

// =============================================================================
// ui::screens::AccountsScreen
//
// Hub for the four account-browsing screens -- All (VaultListScreen),
// Favorites (FavoritesScreen), Categories (CategoriesScreen), Search
// (SearchScreen). Replaces MainMenu's old flat "Vault" item, which
// pushed VaultListScreen directly -- MainMenu's item is now
// "Accounts" and pushes THIS screen instead, matching how Settings
// already works (a hub of sub-screens, not one flat destination).
//
// Not part of docs/GUI.md's own Main Menu section (9) -- that
// document's 7-item list has Vault/Favorites/Categories/Search as
// separate top-level Main Menu items, not nested under one "Accounts"
// hub. Restructured this way at the project owner's request.
// =============================================================================

namespace ui::screens {

class AccountsScreen : public Screen
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
        All,
        Favorites,
        Categories,
        Search,
    };
    static constexpr size_t ITEM_COUNT = 4;

    void render();
    void move_selection(int32_t delta);
    void activate();

    lv_obj_t* item_labels_[ITEM_COUNT]{};
    size_t selected_ = 0;
};

} // namespace ui::screens
