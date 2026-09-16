#include "ui/screens/categories_screen.hpp"

#include "display/fonts.hpp"
#include "ui/screens/account_view_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "vault/vault.hpp"

#include "esp_log.h"

#include <memory>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.categories";

constexpr lv_coord_t FIRST_ITEM_Y = 4;
constexpr lv_coord_t ITEM_SPACING = 20;

// Same reasoning/placeholder status as FavoritesScreen::SCAN_CAP.
constexpr size_t SCAN_CAP = 256;

} // namespace

const char* CategoriesScreen::title() const
{
    return (mode_ == Mode::EntryList) ? "Category" : "Categories";
}

const char* CategoriesScreen::footer_hint() const
{
    return "ROTATE  Select    OK  Open    BACK  Return";
}

void CategoriesScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;
    reload_categories();
}

void CategoriesScreen::on_show()
{
    if (mode_ == Mode::CategoryList) {
        reload_categories();
    } else {
        reload_entries();
    }
}

void CategoriesScreen::reload_categories()
{
    mode_ = Mode::CategoryList;
    lv_obj_clean(content_parent_);
    for (auto& label : category_labels_) {
        label = nullptr;
    }
    category_empty_label_ = nullptr;

    category_count_ = 0;
    uncategorized_present_ = false;

    const size_t total = vault::entry_count();
    const size_t scan_count = (total > SCAN_CAP) ? SCAN_CAP : total;

    if (scan_count > 0) {
        std::vector<vault::VaultEntry> scanned(scan_count);
        vault::list_entries(scanned.data(), scan_count, 0);

        for (const vault::VaultEntry& e : scanned) {
            if (e.category.empty()) {
                uncategorized_present_ = true;
                continue;
            }
            bool already_have = false;
            for (size_t i = 0; i < category_count_; ++i) {
                if (categories_[i] == e.category) {
                    already_have = true;
                    break;
                }
            }
            if (!already_have && category_count_ < MAX_CATEGORIES) {
                categories_[category_count_++] = e.category;
            }
        }
    }

    const size_t total_rows = (uncategorized_present_ ? 1 : 0) + category_count_;
    selected_category_row_ = 0;

    const theme::Palette& pal = theme::current();

    if (total_rows == 0) {
        category_empty_label_ = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(category_empty_label_, pal.secondary_text, 0);
        lv_label_set_text(category_empty_label_, "No categories yet");
        lv_obj_center(category_empty_label_);
    } else {
        for (size_t i = 0; i < total_rows; ++i) {
            lv_obj_t* label = lv_label_create(content_parent_);
            lv_obj_set_style_text_font(label, &keykeeper_cyrillic_16, 0); // category names are user-entered
            lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4,
                         FIRST_ITEM_Y + static_cast<lv_coord_t>(ITEM_SPACING * i));
            category_labels_[i] = label;
        }
    }

    render_categories();
}

void CategoriesScreen::render_categories()
{
    const theme::Palette& pal = theme::current();
    const size_t total_rows = (uncategorized_present_ ? 1 : 0) + category_count_;

    for (size_t row = 0; row < total_rows; ++row) {
        const bool is_selected = (row == selected_category_row_);
        lv_obj_set_style_text_color(category_labels_[row], is_selected ? pal.accent : pal.primary_text, 0);

        const bool is_uncategorized_row = uncategorized_present_ && row == 0;
        const char* prefix = is_selected ? "> " : "";

        if (is_uncategorized_row) {
            lv_label_set_text_fmt(category_labels_[row], "%s(Uncategorized)", prefix);
        } else {
            const size_t idx = row - (uncategorized_present_ ? 1 : 0);
            lv_label_set_text_fmt(category_labels_[row], "%s%s", prefix, categories_[idx].c_str());
        }
    }

    if (total_rows > 0) {
        lv_obj_scroll_to_view(category_labels_[selected_category_row_], LV_ANIM_ON);
    }
}

void CategoriesScreen::move_category_selection(int32_t delta)
{
    const size_t total_rows = (uncategorized_present_ ? 1 : 0) + category_count_;
    if (total_rows == 0) {
        return;
    }

    int32_t index = static_cast<int32_t>(selected_category_row_) + delta;
    const int32_t count = static_cast<int32_t>(total_rows);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_category_row_ = static_cast<size_t>(index);
    render_categories();
}

void CategoriesScreen::enter_selected_category()
{
    const size_t total_rows = (uncategorized_present_ ? 1 : 0) + category_count_;
    if (total_rows == 0) {
        return;
    }

    mode_ = Mode::EntryList;
    reload_entries();
}

void CategoriesScreen::reload_entries()
{
    lv_obj_clean(content_parent_);
    for (auto& label : entry_labels_) {
        label = nullptr;
    }

    const bool is_uncategorized_row = uncategorized_present_ && selected_category_row_ == 0;
    std::string wanted_category;
    if (!is_uncategorized_row) {
        const size_t idx = selected_category_row_ - (uncategorized_present_ ? 1 : 0);
        wanted_category = categories_[idx];
    }

    entries_.clear();

    const size_t total = vault::entry_count();
    const size_t scan_count = (total > SCAN_CAP) ? SCAN_CAP : total;

    if (scan_count > 0) {
        std::vector<vault::VaultEntry> scanned(scan_count);
        vault::list_entries(scanned.data(), scan_count, 0);

        for (const vault::VaultEntry& e : scanned) {
            const bool matches = is_uncategorized_row ? e.category.empty() : (e.category == wanted_category);
            if (matches && entries_.size() < MAX_ROWS) {
                entries_.push_back(e);
            }
        }
    }

    selected_entry_ = 0;

    const theme::Palette& pal = theme::current();
    for (size_t i = 0; i < entries_.size(); ++i) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(label, pal.primary_text, 0);
        lv_obj_set_style_text_font(label, &keykeeper_cyrillic_16, 0); // entry login is user-entered
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4,
                     FIRST_ITEM_Y + static_cast<lv_coord_t>(ITEM_SPACING * i));
        entry_labels_[i] = label;
    }

    render_entries();
}

void CategoriesScreen::render_entries()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < entries_.size(); ++i) {
        const vault::VaultEntry& entry = entries_[i];
        const bool is_selected = (i == selected_entry_);
        const char* login = entry.login.empty() ? "(no login)" : entry.login.c_str();

        lv_obj_set_style_text_color(entry_labels_[i], is_selected ? pal.accent : pal.primary_text, 0);
        lv_label_set_text_fmt(entry_labels_[i], "%s%s", is_selected ? "> " : "", login);
    }

    if (!entries_.empty()) {
        lv_obj_scroll_to_view(entry_labels_[selected_entry_], LV_ANIM_ON);
    }
}

void CategoriesScreen::move_entry_selection(int32_t delta)
{
    if (entries_.empty()) {
        return;
    }

    int32_t index = static_cast<int32_t>(selected_entry_) + delta;
    const int32_t count = static_cast<int32_t>(entries_.size());
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_entry_ = static_cast<size_t>(index);
    render_entries();
}

void CategoriesScreen::activate_entry()
{
    if (entries_.empty()) {
        return;
    }

    const vault::VaultEntry& entry = entries_[selected_entry_];
    ESP_LOGI(TAG, "Selected entry id=%lu login='%s'",
             static_cast<unsigned long>(entry.id), entry.login.c_str());
    manager().push(std::make_unique<AccountViewScreen>(entry.id));
}

bool CategoriesScreen::on_input(InputAction action)
{
    if (mode_ == Mode::EntryList) {
        switch (action) {
            case InputAction::RotateLeft:
                move_entry_selection(-1);
                return true;
            case InputAction::RotateRight:
                move_entry_selection(+1);
                return true;
            case InputAction::OkShort:
                activate_entry();
                return true;
            case InputAction::BackShort:
                reload_categories();
                return true;
            default:
                return false;
        }
    }

    switch (action) {
        case InputAction::RotateLeft:
            move_category_selection(-1);
            return true;
        case InputAction::RotateRight:
            move_category_selection(+1);
            return true;
        case InputAction::OkShort:
            enter_selected_category();
            return true;
        case InputAction::BackShort:
            return false; // pop back to MainMenu
        default:
            return false;
    }
}

} // namespace ui::screens
