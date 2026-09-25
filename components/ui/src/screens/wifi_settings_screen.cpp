#include "ui/screens/wifi_settings_screen.hpp"

#include "display/fonts.hpp"
#include "ui/localization.hpp"
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
    return i18n::tr(i18n::Key::WifiTitle);
}

const char* WifiSettingsScreen::footer_hint() const
{
    switch (mode_) {
        case Mode::Adjust:
            return i18n::tr(i18n::Key::AdjustFooter);
        case Mode::EditText:
            return i18n::tr(i18n::Key::EditFooterTyping);
        default:
            return i18n::tr(i18n::Key::OkOpenBackCancel);
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
    captive_portal_enabled_ = w.captive_portal_enabled;
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
        lv_obj_set_style_text_font(label, &keykeeper_cyrillic_16, 0); // SSID/secret word can be Cyrillic
        // Same fix as usb_settings_screen.cpp's own row labels and
        // ui_manager.cpp's footer_label_ (see that file's comment for
        // the full reasoning) -- a fixed width is required for
        // LV_LABEL_LONG_SCROLL to have anything to detect an overflow
        // against. A row that fits just sits still, same as before.
        lv_obj_set_width(label, LV_PCT(96));
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL);
        // SecretWord and Save (i >= 6) get pushed down one extra
        // ROW_SPACING to make room for status_label_'s own dedicated
        // slot right after CaptivePortal -- see status_label_'s own
        // comment just below for why it moved here instead of staying
        // pinned to the bottom of the screen.
        const lv_coord_t extra_offset = (i >= static_cast<size_t>(Row::SecretWord)) ? ROW_SPACING : 0;
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4,
                     ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i) + extra_offset);
        row_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    // Same width+scroll treatment as the rows above -- the AP-mode
    // status line (IP address plus client count) is exactly the text
    // confirmed on real hardware to not fit and, before this, to just
    // run off the edge instead of scrolling into view.
    lv_obj_set_width(status_label_, LV_PCT(96));
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL);
    // Inline in the scrolling row sequence now (its own dedicated slot
    // between CaptivePortal and SecretWord), NOT pinned to the bottom
    // of the screen -- a fixed-bottom position used to sit UNDER
    // whichever row the scroll-to-view below happened to bring into
    // that same physical spot (confirmed on real hardware: this
    // status text, which can run long -- AP mode shows the IP address
    // and client count -- visibly overlapped the Captive Portal row).
    // Scrolling together with the rest of the content means it can
    // never land on top of a row again, regardless of which one is
    // selected.
    lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 4,
                 ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * static_cast<size_t>(Row::SecretWord)));

    render_rows();
    refresh_status();
}

const char* WifiSettingsScreen::mode_label() const
{
    switch (wifi_mode_) {
        case settings::WifiMode::Disabled:    return i18n::tr(i18n::Key::ModeDisabled);
        case settings::WifiMode::Station:     return i18n::tr(i18n::Key::ModeStation);
        case settings::WifiMode::AccessPoint: return i18n::tr(i18n::Key::ModeAccessPoint);
    }
    return "";
}

const char* WifiSettingsScreen::state_label() const
{
    switch (wifi::state()) {
        case wifi::ConnectionState::Idle:         return i18n::tr(i18n::Key::WifiIdle);
        case wifi::ConnectionState::Connecting:   return i18n::tr(i18n::Key::WifiConnecting);
        case wifi::ConnectionState::Connected:    return i18n::tr(i18n::Key::WifiConnected);
        case wifi::ConnectionState::Disconnected: return i18n::tr(i18n::Key::WifiDisconnected);
        case wifi::ConnectionState::ApRunning:    return i18n::tr(i18n::Key::WifiApRunning);
        case wifi::ConnectionState::Failed:       return i18n::tr(i18n::Key::WifiFailed);
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
        lv_label_set_text_fmt(status_label_, i18n::tr(i18n::Key::WifiStatusFmt), state_label(), wifi::ip_address());
    } else if (state == wifi::ConnectionState::ApRunning) {
        lv_label_set_text_fmt(status_label_, i18n::tr(i18n::Key::WifiStatusApFmt), state_label(), wifi::ip_address(),
                               static_cast<unsigned>(wifi::ap_client_count()),
                               wifi::ap_client_count() == 1 ? "" : "s");
    } else if (state == wifi::ConnectionState::Failed) {
        lv_label_set_text_fmt(status_label_, i18n::tr(i18n::Key::WifiStatusFmt), state_label(), wifi::last_error());
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
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::ModeRowFmt), prefix, mode_label());
                break;
            case Row::StaSsid:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::StationSsidRowFmt), prefix,
                                       sta_ssid_[0] == '\0' ? i18n::tr(i18n::Key::EmptyValue) : sta_ssid_);
                break;
            case Row::StaPassword:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::StationPasswordRowFmt), prefix,
                                       sta_password_[0] == '\0' ? i18n::tr(i18n::Key::EmptyValue) : "********");
                break;
            case Row::ApSsid:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::ApSsidRowFmt), prefix,
                                       ap_ssid_[0] == '\0' ? i18n::tr(i18n::Key::EmptyValue) : ap_ssid_);
                break;
            case Row::ApPassword:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::ApPasswordRowFmt), prefix,
                                       ap_password_[0] == '\0' ? i18n::tr(i18n::Key::OpenValue) : "********");
                break;
            case Row::CaptivePortal:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::CaptivePortalRowFmt), prefix,
                                       captive_portal_enabled_ ? i18n::tr(i18n::Key::OnValue) : i18n::tr(i18n::Key::Off));
                break;
            case Row::SecretWord:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::SecretWordRowFmt), prefix,
                                       secret_word_[0] == '\0' ? i18n::tr(i18n::Key::DisabledValue) : secret_word_);
                break;
            case Row::Save:
                lv_label_set_text_fmt(row_labels_[i], i18n::tr(i18n::Key::SaveApplyRowFmt), prefix);
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
    if (row == Row::CaptivePortal) {
        captive_portal_enabled_ = !captive_portal_enabled_;
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
    // Same bump as the value being typed below it (see
    // widgets::TextEntry's own comment) -- the field NAME deserves
    // the same size increase, not just the value. Width+scroll same
    // as account_edit_screen.cpp's own edit_header_label_ (see that
    // file's comment) -- "Editing: Station Password" and its RU
    // equivalent both genuinely risk running past 320px at this size.
    lv_obj_set_style_text_font(header, &keykeeper_cyrillic_24, 0);
    lv_obj_set_width(header, LV_PCT(96));
    lv_label_set_long_mode(header, LV_LABEL_LONG_SCROLL);

    const char* current_value = "";
    const char* label_text = "";
    switch (row) {
        case Row::StaSsid:     current_value = sta_ssid_;     label_text = i18n::tr(i18n::Key::EditingStationSsid); break;
        case Row::StaPassword: current_value = sta_password_; label_text = i18n::tr(i18n::Key::EditingStationPassword); break;
        case Row::ApSsid:      current_value = ap_ssid_;       label_text = i18n::tr(i18n::Key::EditingApSsid); break;
        case Row::ApPassword:  current_value = ap_password_;   label_text = i18n::tr(i18n::Key::EditingApPassword); break;
        case Row::SecretWord:  current_value = secret_word_;   label_text = i18n::tr(i18n::Key::EditingSecretWord); break;
        default: break;
    }
    lv_label_set_text(header, label_text);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 4, 4);

    widgets::TextEntry::Config cfg{};
    cfg.max_length = (row == Row::StaPassword || row == Row::ApPassword) ? 64 : 32;
    // NOT masked, even for the password rows -- this whole screen is
    // already behind the device's own PIN, and the project owner's
    // own call: being unable to actually READ the current value while
    // editing it (only seeing mask dots) defeats the point of
    // pre-filling it at all -- there's no way to tell what you're
    // changing FROM. Vault entries' own passwords stay masked
    // elsewhere; this is specifically about local WiFi credentials on
    // an already-unlocked device.
    cfg.mask = false;
    // Password fields specifically, not SSID or Secret Word -- see
    // Config::allow_cyrillic's own comment for the reasoning.
    cfg.allow_cyrillic = !(row == Row::StaPassword || row == Row::ApPassword);
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
    updated.captive_portal_enabled = captive_portal_enabled_;

    if (!settings::set_wifi(updated)) {
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::SaveFailed));
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
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::SaveFailed));
        return;
    }

    ESP_LOGI(TAG, "WiFi settings saved, applying...");
    if (!wifi::apply_settings()) {
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::SavedButFailedToApply));
        return;
    }

    // Routes bake the current secret word in as a literal path
    // prefix at registration time -- restart so a changed word takes
    // effect immediately rather than only on next boot.
    web::restart();

    // Same pattern every sibling settings screen already follows on a
    // successful save (GeneralSettingsScreen, SecuritySettingsScreen,
    // etc. all manager().pop() here) -- this screen was the one
    // inconsistent holdout, confirmed on real hardware as a real,
    // noticeable difference in behavior, not just a style nitpick.
    // refresh_status() no longer needed right after -- there's no one
    // left to see status_label_ once this screen is gone.
    manager().pop();
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
