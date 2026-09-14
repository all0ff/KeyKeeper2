#pragma once

#include "ui/screen.hpp"
#include "ui/widgets/text_entry.hpp"

#include "settings/settings_types.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::WifiSettingsScreen
//
// Not part of docs/GUI.md section 14's original four sections
// (General/USB/Security/System) -- that document predates
// components/wifi entirely. Added as a fifth SettingsScreen item,
// same list-of-sections pattern as the other four.
//
// Mode (settings::WifiMode: Disabled/Station/AccessPoint) is cycled
// like GeneralSettingsScreen's Language row; Station and Access Point
// each get their own SSID/password fields via widgets::TextEntry
// (passwords masked, like AccountEditScreen's Password field) --
// both sets of fields are always shown regardless of the current
// mode, since switching modes shouldn't require re-entering
// credentials you already typed for the other one.
//
// Save persists via settings::set_wifi() AND calls
// wifi::apply_settings() to actually bring the new configuration up
// immediately -- unlike the other Settings screens, where Save is
// purely a settings::set_*() call and nothing about the running
// firmware changes until next boot, here the whole point is to
// (re)connect right away.
//
// Secret Word row: from KeyKeeper 1.90's own "secretword" feature
// (see settings::SecuritySettings::secret_word's doc comment) -- an
// optional URL path-prefix requirement for the Web UI/REST API.
// Grouped here (not in SecuritySettingsScreen, which is already
// large) because it's fundamentally about WEB access, even though the
// underlying field lives in settings::SecuritySettings, not
// settings::WifiSettings -- Save writes BOTH sections and also calls
// web::restart() so a changed word takes effect immediately (routes
// bake the current word in as a literal path prefix at registration
// time).
// =============================================================================

namespace ui::screens {

class WifiSettingsScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Row : uint8_t
    {
        Mode,
        StaSsid,
        StaPassword,
        ApSsid,
        ApPassword,
        SecretWord,
        Save,
    };
    static constexpr size_t ROW_COUNT = 7;

    enum class Mode : uint8_t
    {
        Browse,
        Adjust,
        EditText,
    };

    void build_rows(lv_obj_t* parent);
    void render_rows();
    void refresh_status();
    void move_selection(int32_t delta);
    void adjust_value(int32_t delta);
    void activate();
    void enter_edit_text(Row row);
    void exit_edit_text(bool commit);
    void save();

    const char* mode_label() const;
    const char* state_label() const;

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    size_t selected_row_ = 0;
    Mode mode_ = Mode::Browse;
    Row editing_row_ = Row::StaSsid;

    // Working copy -- persisted (and applied) via save().
    settings::WifiMode wifi_mode_ = settings::WifiMode::Disabled;
    char sta_ssid_[33]{};
    char sta_password_[65]{};
    char ap_ssid_[33]{};
    char ap_password_[65]{};
    char secret_word_[33]{};

    widgets::TextEntry text_entry_;
};

} // namespace ui::screens
