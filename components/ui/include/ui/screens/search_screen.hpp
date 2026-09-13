#pragma once

#include "ui/screen.hpp"
#include "ui/widgets/text_entry.hpp"

#include "vault/vault_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

// =============================================================================
// ui::screens::SearchScreen
//
// docs/GUI.md section 13: search by name/URL/login, dynamically
// updating results, empty query shows the full list. "Name" maps to
// VaultEntry::login as everywhere else in this UI; search also
// includes Notes (not explicitly listed in GUI.md 13, a reasonable
// addition given the field exists and free-text search over it costs
// nothing extra). Category filter and "show favorites" (also listed
// in GUI.md 13) are NOT folded into this screen -- they're already
// their own dedicated screens (CategoriesScreen, FavoritesScreen);
// duplicating that here isn't a considered omission, just not
// repeated in two places.
//
// Two modes, since one encoder can't both spin characters and
// navigate a result list at the same time:
//   - Typing (default): widgets::TextEntry as normal. Every
//     character committed (OkShort) or erased (BackShort) reruns the
//     search immediately and re-renders the result list below the
//     query -- this is the "updates dynamically" part. OkLong
//     switches to Browsing.
//   - Browsing: rotate/OK navigate and open the results (same as
//     every other list screen here); BackShort returns to Typing to
//     keep editing the query. BackShort in Typing with an empty query
//     leaves the screen entirely (default nav).
//
// Case-insensitive substring match, not fuzzy/ranked search.
// =============================================================================

namespace ui::screens {

class SearchScreen : public Screen
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
        Typing,
        Browsing,
    };

    void update_results();
    void render_results();
    void move_result_selection(int32_t delta);
    void activate_result();
    void enter_browsing();
    void enter_typing();

    lv_obj_t* content_parent_ = nullptr;
    Mode mode_ = Mode::Typing;

    widgets::TextEntry query_entry_;
    lv_obj_t* status_label_ = nullptr;

    static constexpr size_t MAX_ROWS = 24;
    static constexpr size_t SCAN_CAP = 256; // same placeholder reasoning as FavoritesScreen

    std::vector<vault::VaultEntry> results_;
    lv_obj_t* result_labels_[MAX_ROWS]{};
    size_t selected_result_ = 0;

    // Only update_results() when query_entry_.text() actually changed
    // since the last check -- on_input() is called on every rotate
    // detent too (just spinning the character wheel, not yet
    // committed), and rebuilding the whole result list's LVGL objects
    // on every one of those would be wasteful and could visibly
    // flicker on real hardware.
    std::string last_query_;
};

} // namespace ui::screens
