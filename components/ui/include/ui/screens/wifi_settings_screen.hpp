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
//
// Captive Portal is NOT its own row anymore -- it's a value of the Mode
// row: Disabled / Access Point / Access Point + CP / Station (that
// cycle order, see wifi_settings_screen.cpp's ModeChoice). "Access
// Point + CP" means mode == AccessPoint AND
// settings::WifiSettings::captive_portal_enabled, which starts
// wifi::captive_dns on top of the plain AP -- see that component's
// own file comment for what it does. Nothing about the STORED data
// changed: still the same settings::WifiMode enum plus the same bool,
// so existing NVS blobs, the Web UI's own separate checkbox, and
// wifi_service.cpp all keep working untouched -- this screen just
// presents the (mode, bool) pair as one choice. A Lite build has no
// captive portal, so its cycle simply has no "Access Point + CP"
// entry.
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

    // Adjust mode is "OK confirms, BACK cancels": begin_adjust()
    // snapshots every value adjust_value() can change, end_adjust(true)
    // keeps whatever the encoder left them at, end_adjust(false) puts
    // them back exactly as they were when this row was entered. Only
    // THIS row's pending change is discarded -- earlier confirmed
    // changes on other rows stay in the working copy until the
    // screen's own Save (or leaving it, which discards them all).
    void begin_adjust();
    void end_adjust(bool commit);

    struct AdjustSnapshot
    {
        settings::WifiMode wifi_mode;
        bool captive_portal_enabled;
    };
    AdjustSnapshot adjust_snapshot_{};
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
    // Default false, NOT true: initialize() assigns it explicitly in
    // both variants, but a Lite build never reads it from settings::
    // (see there) -- relying on a "true" default here would make a Lite
    // device in AccessPoint mode display "Access Point + CP", a choice
    // Lite's own cycle doesn't contain.
    bool captive_portal_enabled_ = false;
    char secret_word_[33]{};

    widgets::TextEntry text_entry_;
};

} // namespace ui::screens
