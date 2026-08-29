#pragma once

#include "ui/screen.hpp"

#include "vault/vault_model.hpp"

#include <cstddef>
#include <vector>

// =============================================================================
// ui::screens::VaultListScreen
//
// docs/GUI.md section 10 ("Vault Screen") -- the main working screen,
// lists saved accounts. Pushed from MainMenu's "Vault" item.
//
// docs/GUI.md 10 says each row may show: name, category, URL, an OTP
// indicator, and a favorite flag. vault::VaultEntry (see
// vault_model.hpp) does not have category or favorite fields at all
// -- those were never part of the data model the vault component
// actually built. This screen shows what the model has: login (used
// as the row's identifying name -- there's no separate "title"
// field), and an [OTP] tag derived from whether totp_secret is
// non-empty. Category/favorite are simply not displayed; not a bug,
// the data to display doesn't exist.
//
// Selecting an entry (OkShort) pushes ui::screens::AccountViewScreen
// with the entry's id. Holding OK (OkLong) pushes
// ui::screens::AccountEditScreen with vault::INVALID_ID to create a
// new entry -- there is no dedicated "+ Add" row in the list.
//
// Rows are hand-positioned the same way ui::screens::MainMenu does
// (no shared list widget -- see components/ui/README.md), scrolled
// into view on selection change via the content container's default
// LVGL scrolling.
// =============================================================================

namespace ui::screens {

class VaultListScreen : public Screen
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

    /// Upper bound on how many entries this screen will render.
    /// docs/GUI.md doesn't specify a limit or how a large vault
    /// should be handled (scrolling in pages, search-first, ...) --
    /// this is a placeholder cap, not a considered design. A vault
    /// with more entries than this simply won't show the rest; see
    /// components/ui/README.md.
    static constexpr size_t MAX_ROWS = 32;

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* row_labels_[MAX_ROWS]{};
    lv_obj_t* empty_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;

    std::vector<vault::VaultEntry> entries_;
    size_t selected_ = 0;
};

} // namespace ui::screens
