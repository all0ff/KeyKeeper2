#include "ui/screens/wifi_settings_screen.hpp"

#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "settings/settings.hpp"
#include "web/web_service.hpp"
#include "wifi/wifi_service.hpp"

#include "esp_log.h"

#include <cstring>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.wifi_settings";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

} // namespace

const char* WifiSettingsScreen::title() const
{
    return "WiFi";
}

const char* WifiSettingsScreen::footer_hint() const
{
    switch (mode_) {
        case Mode::Adjust:
            return "ROTATE  Change    OK/BACK  Confirm";
        case Mode::EditText:
            return "OK  Add char    Hold OK  Done    BACK  Erase    Hold BACK  Switch set";
        default:
            return "OK  Open    BACK  Cancel";
    }
}

void WifiSettingsScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    const settings::WifiSettings& w = settings::all().wifi;
    wifi_mode_ = w.mode;
    std::strncpy(sta_ssid_, w.sta_ssid, sizeof(sta_ssid_) - 1);
    std::strncpy(sta_password_, w.sta_password, sizeof(sta_password_) - 1);
    std::strncpy(ap_ssid_, w.ap_ssid, sizeof(ap_ssid_) - 1);
    std::strncpy(ap_password_, w.ap_password, sizeof(ap_password_) - 1);
    std::strncpy(secret_word_, settings::all().security.secret_word, sizeof(secret_word_) - 1);

    build_rows(content_parent_);
}

void WifiSettingsScreen::on_show()
{
    refresh_status();
}

void WifiSettingsScreen::build_rows(lv_obj_t* parent)
{
    for (size_t i = 0; i < ROW_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(parent);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        row_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_rows();
    refresh_status();
}

const char* WifiSettingsScreen::mode_label() const
{
    switch (wifi_mode_) {
        case settings::WifiMode::Disabled:    return "Disabled";
        case settings::WifiMode::Station:     return "Station";
        case settings::WifiMode::AccessPoint: return "Access Point";
    }
    return "";
}

const char* WifiSettingsScreen::state_label() const
{
    switch (wifi::state()) {
        case wifi::ConnectionState::Idle:         return "Idle";
        case wifi::ConnectionState::Connecting:   return "Connecting...";
        case wifi::ConnectionState::Connected:    return "Connected";
        case wifi::ConnectionState::Disconnected: return "Disconnected";
        case wifi::ConnectionState::ApRunning:    return "AP running";
        case wifi::ConnectionState::Failed:       return "Failed";
    }
    return "";
}

void WifiSettingsScreen::refresh_status()
{
    if (status_label_ == nullptr) {
        return;
    }

    const wifi::ConnectionState state = wifi::state();

    if (state == wifi::ConnectionState::Connected) {
        lv_label_set_text_fmt(status_label_, "%s: %s", state_label(), wifi::ip_address());
    } else if (state == wifi::ConnectionState::ApRunning) {
        lv_label_set_text_fmt(status_label_, "%s: %s (%u client%s)", state_label(), wifi::ip_address(),
                               static_cast<unsigned>(wifi::ap_client_count()),
                               wifi::ap_client_count() == 1 ? "" : "s");
    } else if (state == wifi::ConnectionState::Failed) {
        lv_label_set_text_fmt(status_label_, "%s: %s", state_label(), wifi::last_error());
    } else {
        lv_label_set_text(status_label_, state_label());
    }
}

void WifiSettingsScreen::render_rows()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool is_selected = (i == selected_row_);
        const bool is_adjusting = is_selected && mode_ == Mode::Adjust;
        lv_obj_set_style_text_color(
            row_labels_[i], is_adjusting ? pal.warning : (is_selected ? pal.accent : pal.primary_text), 0);

        const char* prefix = is_selected ? "> " : "";

        switch (static_cast<Row>(i)) {
            case Row::Mode:
                lv_label_set_text_fmt(row_labels_[i], "%sMode: %s", prefix, mode_label());
                break;
            case Row::StaSsid:
                lv_label_set_text_fmt(row_labels_[i], "%sStation SSID: %s", prefix,
                                       sta_ssid_[0] == '\0' ? "(empty)" : sta_ssid_);
                break;
            case Row::StaPassword:
                lv_label_set_text_fmt(row_labels_[i], "%sStation Password: %s", prefix,
                                       sta_password_[0] == '\0' ? "(empty)" : "********");
                break;
            case Row::ApSsid:
                lv_label_set_text_fmt(row_labels_[i], "%sAP SSID: %s", prefix,
                                       ap_ssid_[0] == '\0' ? "(empty)" : ap_ssid_);
                break;
            case Row::ApPassword:
                lv_label_set_text_fmt(row_labels_[i], "%sAP Password: %s", prefix,
                                       ap_password_[0] == '\0' ? "(open)" : "********");
                break;
            case Row::SecretWord:
                lv_label_set_text_fmt(row_labels_[i], "%sSecret Word: %s", prefix,
                                       secret_word_[0] == '\0' ? "(disabled)" : secret_word_);
                break;
            case Row::Save:
                lv_label_set_text_fmt(row_labels_[i], "%sSave & Apply", prefix);
                break;
        }
    }

    // 7 rows don't all fit in the visible content area at once --
    // keep the selected one scrolled into view as it moves (this
    // used to be missing entirely, which is exactly what caused Save
    // to render mostly hidden behind status_label_ at the bottom).
    if (row_labels_[selected_row_] != nullptr) {
        lv_obj_scroll_to_view(row_labels_[selected_row_], LV_ANIM_ON);
    }
}

void WifiSettingsScreen::move_selection(int32_t delta)
{
    int32_t index = static_cast<int32_t>(selected_row_) + delta;
    const int32_t count = static_cast<int32_t>(ROW_COUNT);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_row_ = static_cast<size_t>(index);
    render_rows();
}

void WifiSettingsScreen::adjust_value(int32_t delta)
{
    if (static_cast<Row>(selected_row_) != Row::Mode) {
        return;
    }

    // Cycle Disabled -> Station -> AccessPoint -> Disabled ...
    int value = static_cast<int>(wifi_mode_) + delta;
    constexpr int MODE_COUNT = 3;
    if (value < 0) {
        value = MODE_COUNT - 1;
    }
    if (value >= MODE_COUNT) {
        value = 0;
    }
    wifi_mode_ = static_cast<settings::WifiMode>(value);
    render_rows();
}

void WifiSettingsScreen::activate()
{
    const auto row = static_cast<Row>(selected_row_);

    if (row == Row::Save) {
        save();
        return;
    }
    if (row == Row::Mode) {
        mode_ = Mode::Adjust;
        render_rows();
        return;
    }

    enter_edit_text(row);
}

void WifiSettingsScreen::enter_edit_text(Row row)
{
    editing_row_ = row;
    mode_ = Mode::EditText;

    lv_obj_clean(content_parent_);

    const theme::Palette& pal = theme::current();
    lv_obj_t* header = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(header, pal.secondary_text, 0);

    const char* current_value = "";
    const char* label_text = "";
    switch (row) {
        case Row::StaSsid:     current_value = sta_ssid_;     label_text = "Editing: Station SSID"; break;
        case Row::StaPassword: current_value = sta_password_; label_text = "Editing: Station Password"; break;
        case Row::ApSsid:      current_value = ap_ssid_;       label_text = "Editing: AP SSID"; break;
        case Row::ApPassword:  current_value = ap_password_;   label_text = "Editing: AP Password"; break;
        case Row::SecretWord:  current_value = secret_word_;   label_text = "Editing: Secret Word"; break;
        default: break;
    }
    lv_label_set_text(header, label_text);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 4, 4);

    widgets::TextEntry::Config cfg{};
    cfg.max_length = (row == Row::StaPassword || row == Row::ApPassword) ? 64 : 32;
    cfg.mask = (row == Row::StaPassword || row == Row::ApPassword);
    cfg.initial_value = current_value;

    text_entry_.init(content_parent_, cfg);
    lv_obj_align(text_entry_.root(), LV_ALIGN_TOP_LEFT, 4, 24);
}

void WifiSettingsScreen::exit_edit_text(bool commit)
{
    if (commit) {
        char* target = nullptr;
        size_t target_size = 0;
        switch (editing_row_) {
            case Row::StaSsid:     target = sta_ssid_;     target_size = sizeof(sta_ssid_); break;
            case Row::StaPassword: target = sta_password_; target_size = sizeof(sta_password_); break;
            case Row::ApSsid:      target = ap_ssid_;       target_size = sizeof(ap_ssid_); break;
            case Row::ApPassword:  target = ap_password_;   target_size = sizeof(ap_password_); break;
            case Row::SecretWord:  target = secret_word_;   target_size = sizeof(secret_word_); break;
            default: break;
        }
        if (target != nullptr) {
            std::strncpy(target, text_entry_.text(), target_size - 1);
            target[target_size - 1] = '\0';
        }
    }

    mode_ = Mode::Browse;
    lv_obj_clean(content_parent_);
    build_rows(content_parent_);
}

void WifiSettingsScreen::save()
{
    settings::WifiSettings updated;
    updated.mode = wifi_mode_;
    std::strncpy(updated.sta_ssid, sta_ssid_, sizeof(updated.sta_ssid) - 1);
    std::strncpy(updated.sta_password, sta_password_, sizeof(updated.sta_password) - 1);
    std::strncpy(updated.ap_ssid, ap_ssid_, sizeof(updated.ap_ssid) - 1);
    std::strncpy(updated.ap_password, ap_password_, sizeof(updated.ap_password) - 1);

    if (!settings::set_wifi(updated)) {
        lv_label_set_text(status_label_, "Save failed");
        return;
    }

    // secret_word lives in settings::SecuritySettings, not
    // settings::WifiSettings -- see this screen's own file comment
    // for why it's edited here anyway. Only that one field is
    // touched; everything else in SecuritySettings is carried over
    // as-is.
    settings::SecuritySettings sec = settings::all().security;
    std::strncpy(sec.secret_word, secret_word_, sizeof(sec.secret_word) - 1);
    if (!settings::set_security(sec)) {
        lv_label_set_text(status_label_, "Save failed");
        return;
    }

    ESP_LOGI(TAG, "WiFi settings saved, applying...");
    if (!wifi::apply_settings()) {
        lv_label_set_text(status_label_, "Saved, but failed to apply");
        return;
    }

    // Routes bake the current secret word in as a literal path
    // prefix at registration time -- restart so a changed word takes
    // effect immediately rather than only on next boot.
    web::restart();

    refresh_status();
}

bool WifiSettingsScreen::on_input(InputAction action)
{
    if (mode_ == Mode::EditText) {
        const bool consumed = text_entry_.on_input(action);

        if (text_entry_.is_finished()) {
            exit_edit_text(true);
            return true;
        }
        if (!consumed) {
            exit_edit_text(false);
            return true;
        }
        return true;
    }

    if (mode_ == Mode::Adjust) {
        switch (action) {
            case InputAction::RotateLeft:
                adjust_value(-1);
                return true;
            case InputAction::RotateRight:
                adjust_value(+1);
                return true;
            case InputAction::OkShort:
            case InputAction::BackShort:
                mode_ = Mode::Browse;
                render_rows();
                return true;
            default:
                return false;
        }
    }

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
            return false; // pop, discarding unsaved changes

        default:
            return false;
    }
}

} // namespace ui::screens
