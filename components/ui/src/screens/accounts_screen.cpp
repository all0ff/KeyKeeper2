#include "ui/screens/accounts_screen.hpp"

#include "ui/localization.hpp"
#include "ui/screens/categories_screen.hpp"
#include "ui/screens/favorites_screen.hpp"
#include "ui/screens/search_screen.hpp"
#include "ui/screens/vault_list_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include <memory>

namespace ui::screens {

namespace {

constexpr lv_coord_t ITEM_Y_START = 4;
constexpr lv_coord_t ITEM_SPACING = 20;

// NOT constexpr -- i18n::tr() reads the current language at runtime,
// so this has to be built fresh each render() call rather than once
// at compile time (a language change mid-session must be reflected
// immediately, same as every other translated screen).
const char* item_name(size_t i)
{
    switch (i) {
        case 0: return i18n::tr(i18n::Key::All);
        case 1: return i18n::tr(i18n::Key::Favorites);
        case 2: return i18n::tr(i18n::Key::Categories);
        case 3: return i18n::tr(i18n::Key::Search);
        default: return "";
    }
}

} // namespace

const char* AccountsScreen::title() const
{
    return i18n::tr(i18n::Key::Accounts);
}

const char* AccountsScreen::footer_hint() const
{
    return i18n::tr(i18n::Key::OkOpenBackReturn);
}

void AccountsScreen::initialize(lv_obj_t* content_parent)
{
    for (size_t i = 0; i < ITEM_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(content_parent);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ITEM_Y_START + static_cast<lv_coord_t>(ITEM_SPACING * i));
        item_labels_[i] = label;
    }

    render();
}

void AccountsScreen::on_show()
{
    render();
}

void AccountsScreen::render()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ITEM_COUNT; ++i) {
        const bool is_selected = (i == selected_);
        lv_obj_set_style_text_color(item_labels_[i], is_selected ? pal.accent : pal.primary_text, 0);
        lv_label_set_text_fmt(item_labels_[i], "%s%s", is_selected ? "> " : "", item_name(i));
    }
}

void AccountsScreen::move_selection(int32_t delta)
{
    int32_t index = static_cast<int32_t>(selected_) + delta;
    const int32_t count = static_cast<int32_t>(ITEM_COUNT);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_ = static_cast<size_t>(index);
    render();
}

void AccountsScreen::activate()
{
    switch (static_cast<Item>(selected_)) {
        case Item::All:
            manager().push(std::make_unique<VaultListScreen>());
            return;

        case Item::Favorites:
            manager().push(std::make_unique<FavoritesScreen>());
            return;

        case Item::Categories:
            manager().push(std::make_unique<CategoriesScreen>());
            return;

        case Item::Search:
            manager().push(std::make_unique<SearchScreen>());
            return;
    }
}

bool AccountsScreen::on_input(InputAction action)
{
    switch (action) {
        case InputAction::RotateLeft:
            move_selection(-1);
            return true;

        case InputAction::RotateRight:
            move_selection(+1);
            return true;

        case InputAction::OkShort:
            activate();
            return true;

        case InputAction::BackShort:
            return false; // pop back to MainMenu

        default:
            return false;
    }
}

} // namespace ui::screens
