#include "ui/screens/search_screen.hpp"

#include "display/fonts.hpp"
#include "ui/localization.hpp"
#include "ui/screens/account_view_screen.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "vault/vault.hpp"

#include "esp_log.h"

#include <cctype>
#include <cstring>
#include <memory>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.search";

constexpr lv_coord_t QUERY_Y = 4;
constexpr lv_coord_t STATUS_Y = 24;
constexpr lv_coord_t RESULTS_Y_START = 40;
constexpr lv_coord_t ROW_SPACING = 18;

} // namespace

const char* SearchScreen::title() const
{
    return i18n::tr(i18n::Key::Search);
}

const char* SearchScreen::footer_hint() const
{
    if (mode_ == Mode::Browsing) {
        return i18n::tr(i18n::Key::SearchFooterBrowse);
    }
    return i18n::tr(i18n::Key::SearchFooterTyping);
}

void SearchScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    widgets::TextEntry::Config cfg{};
    cfg.max_length = 32; // a search query doesn't need vault::MAX_*_LEN-sized capacity
    query_entry_.init(content_parent_, cfg);
    lv_obj_align(query_entry_.root(), LV_ALIGN_TOP_LEFT, 4, QUERY_Y);

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 4, STATUS_Y);

    last_query_ = "\x01"; // sentinel guaranteed to differ from any real query -- forces the first update_results()
    update_results();
}

void SearchScreen::on_show()
{
    // Intentionally does not reset the query -- returning here (e.g.
    // BACK from an AccountViewScreen this screen opened) keeps
    // whatever was being searched for.
    update_results();
}

void SearchScreen::update_results()
{
    const std::string current_query(query_entry_.text());
    if (current_query == last_query_) {
        return; // nothing actually changed -- see the header comment
    }
    last_query_ = current_query;

    for (size_t i = 0; i < MAX_ROWS; ++i) {
        if (result_labels_[i] != nullptr) {
            lv_obj_del(result_labels_[i]);
            result_labels_[i] = nullptr;
        }
    }
    results_.clear();

    // vault::search_entries() scans the WHOLE vault (no SCAN_CAP-style
    // ceiling on what's considered) and caps only the RESULT count at
    // MAX_ROWS -- previously this screen capped what it even
    // considered via SCAN_CAP, which could miss real matches sitting
    // past that scan limit even when there was room left for more
    // results. Same case-insensitive login/url/notes matching either
    // way -- shared with the web search REST endpoint now, not a
    // separate copy of the same logic.
    // Heap-allocated, NOT a stack array -- same class of bug as
    // vault_csv.cpp's export_csv() (confirmed as a real "stack
    // overflow in task ui" panic + reboot on real hardware) and
    // vault.cpp's own search_entries() internal scan buffer, both
    // fixed alongside this one -- MAX_ROWS (24) of these on this
    // screen's own "ui" task stack is even larger than either of
    // those.
    std::vector<vault::VaultEntry> buf(MAX_ROWS);
    const size_t found = vault::search_entries(current_query.c_str(), buf.data(), MAX_ROWS);
    for (size_t i = 0; i < found; ++i) {
        results_.push_back(buf[i]);
    }

    selected_result_ = 0;

    const theme::Palette& pal = theme::current();
    for (size_t i = 0; i < results_.size(); ++i) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(label, pal.primary_text, 0);
        // Result text comes from matched entries' own fields
        // (user-entered, could be Cyrillic).
        lv_obj_set_style_text_font(label, &keykeeper_cyrillic_16, 0);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4,
                     RESULTS_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        result_labels_[i] = label;
    }

    if (results_.empty()) {
        lv_label_set_text(status_label_, current_query.empty() ? i18n::tr(i18n::Key::VaultEmpty) : i18n::tr(i18n::Key::NoMatches));
    } else {
        lv_label_set_text_fmt(status_label_, "%u result%s", static_cast<unsigned>(results_.size()),
                               results_.size() == 1 ? "" : "s");
    }

    render_results();
}

void SearchScreen::render_results()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < results_.size(); ++i) {
        const vault::VaultEntry& entry = results_[i];
        const bool is_selected = (mode_ == Mode::Browsing) && (i == selected_result_);
        const char* login = entry.login.empty() ? "(no login)" : entry.login.c_str();

        lv_obj_set_style_text_color(result_labels_[i], is_selected ? pal.accent : pal.primary_text, 0);
        lv_label_set_text_fmt(result_labels_[i], "%s%s%s",
                               is_selected ? "> " : "", entry.favorite ? "* " : "", login);
    }

    if (mode_ == Mode::Browsing && !results_.empty()) {
        lv_obj_scroll_to_view(result_labels_[selected_result_], LV_ANIM_ON);
    }
}

void SearchScreen::move_result_selection(int32_t delta)
{
    if (results_.empty()) {
        return;
    }

    int32_t index = static_cast<int32_t>(selected_result_) + delta;
    const int32_t count = static_cast<int32_t>(results_.size());
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_result_ = static_cast<size_t>(index);
    render_results();
}

void SearchScreen::activate_result()
{
    if (results_.empty()) {
        return;
    }

    const vault::VaultEntry& entry = results_[selected_result_];
    ESP_LOGI(TAG, "Selected search result id=%lu login='%s'",
             static_cast<unsigned long>(entry.id), entry.login.c_str());
    manager().push(std::make_unique<AccountViewScreen>(entry.id));
}

void SearchScreen::enter_browsing()
{
    // Consume TextEntry's "finished" signal without losing the typed
    // query. reset(initial_value) can't safely be passed
    // query_entry_.text() directly: it clears the buffer BEFORE
    // copying from initial_value, and text() points INTO that same
    // buffer -- passing it straight through would be a self-copy that
    // just erases the query. Copy to a local buffer first.
    char saved[130];
    std::strncpy(saved, query_entry_.text(), sizeof(saved) - 1);
    saved[sizeof(saved) - 1] = '\0';
    query_entry_.reset(saved);

    mode_ = Mode::Browsing;
    selected_result_ = 0;
    render_results();
}

void SearchScreen::enter_typing()
{
    mode_ = Mode::Typing;
    render_results(); // clears the "> " marker -- Typing mode never shows a selected result
}

bool SearchScreen::on_input(InputAction action)
{
    if (mode_ == Mode::Browsing) {
        switch (action) {
            case InputAction::RotateLeft:
                move_result_selection(-1);
                return true;
            case InputAction::RotateRight:
                move_result_selection(+1);
                return true;
            case InputAction::OkShort:
                activate_result();
                return true;
            case InputAction::BackShort:
                enter_typing();
                return true;
            default:
                return false;
        }
    }

    // Typing mode.
    const bool consumed = query_entry_.on_input(action);

    if (query_entry_.is_finished()) {
        enter_browsing();
        return true;
    }

    if (!consumed) {
        // BackShort with an empty query -- leave the screen entirely,
        // same "nothing left to erase" convention used everywhere
        // else TextEntry/PinEntry are used.
        return false;
    }

    update_results();
    return true;
}

} // namespace ui::screens
