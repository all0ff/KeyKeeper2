#pragma once

#include "ui/screen.hpp"

#include "vault/vault_model.hpp"

#include <cstddef>
#include <vector>

// =============================================================================
// ui::screens::FavoritesScreen
//
// Filtered view of the vault: only entries with favorite == true
// (format v2, see vault_model.hpp). Same interaction model as
// VaultListScreen (rotate/select/OK opens AccountViewScreen), just a
// different, smaller source list -- no "hold OK to create a new
// entry" here, since a fresh entry starts unfavorited and wouldn't
// show up in this list anyway; create new entries from VaultListScreen.
//
// Same MAX_ROWS placeholder cap as VaultListScreen -- see its header
// comment.
// =============================================================================

namespace ui::screens {

class FavoritesScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    void reload();
    void render();
    void move_selection(int32_t delta);
    void activate();

    static constexpr size_t MAX_ROWS = 32;

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* row_labels_[MAX_ROWS]{};
    lv_obj_t* empty_label_ = nullptr;

    std::vector<vault::VaultEntry> entries_;
    size_t selected_ = 0;
};

} // namespace ui::screens
