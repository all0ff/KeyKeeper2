#pragma once

#include "ui/screen.hpp"

#include "vault/vault_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

// =============================================================================
// ui::screens::CategoriesScreen
//
// Two-level browse: first a list of distinct category values found
// across the vault (plus a synthetic "(Uncategorized)" entry if any
// account has an empty category), then the accounts within whichever
// one is selected. Selecting an account there pushes
// AccountViewScreen, same as everywhere else.
//
// Categories are plain free-text (vault_model.hpp's
// VaultEntry::category) -- this is flat string grouping, not a
// hierarchy or a separate managed list of allowed category names;
// whatever strings AccountEditScreen has actually saved are what show
// up here. BACK from the entry list returns to the category list;
// BACK from the category list returns to MainMenu.
// =============================================================================

namespace ui::screens {

class CategoriesScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Mode : uint8_t
    {
        CategoryList,
        EntryList,
    };

    void reload_categories();
    void render_categories();
    void move_category_selection(int32_t delta);
    void enter_selected_category();

    void reload_entries();
    void render_entries();
    void move_entry_selection(int32_t delta);
    void activate_entry();

    lv_obj_t* content_parent_ = nullptr;
    Mode mode_ = Mode::CategoryList;

    static constexpr size_t MAX_CATEGORIES = 16;
    static constexpr size_t MAX_ROWS = 32;

    // Row 0 is "(Uncategorized)" when uncategorized_present_ is true;
    // categories_[0..category_count_) are the real category strings
    // either way (rendered starting one row lower in that case).
    std::string categories_[MAX_CATEGORIES];
    size_t category_count_ = 0;
    bool uncategorized_present_ = false;
    lv_obj_t* category_labels_[MAX_CATEGORIES + 1]{};
    lv_obj_t* category_empty_label_ = nullptr;
    size_t selected_category_row_ = 0;

    std::vector<vault::VaultEntry> entries_;
    lv_obj_t* entry_labels_[MAX_ROWS]{};
    size_t selected_entry_ = 0;
};

} // namespace ui::screens
