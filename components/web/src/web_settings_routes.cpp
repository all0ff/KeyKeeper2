#include "web_settings_routes.hpp"
#include "web_json_helpers.hpp"

#include "security/lock_manager.hpp"
#include "settings/settings.hpp"
#include "wifi/wifi_service.hpp"
#include "display/display.hpp"

#include "cJSON.h"
#include "esp_log.h"

#include <cstring>
#include <string>

namespace web {

namespace {

constexpr char TAG[] = "web.settings";

bool require_unlocked(httpd_req_t* req)
{
    if (security::lock::state() != security::lock::State::Unlocked) {
        respond_error(req, "401 Unauthorized", "Not authenticated");
        return false;
    }
    // See security::lock::notify_activity()'s own comment -- a real,
    // confirmed-on-hardware bug: without this, using the device
    // purely through the Web UI still auto-locked on the physical
    // input timer's own schedule, and the very next request after a
    // web-based unlock would immediately auto-lock again since
    // nothing about that unlock had touched input's own activity
    // timestamp.
    security::lock::notify_activity();
    return true;
}

// -----------------------------------------------------------------
// Enum <-> string mappings -- JSON carries these as readable strings,
// not raw enum integers, for anyone poking the API by hand.
// -----------------------------------------------------------------

const char* language_to_string(settings::Language l)
{
    return (l == settings::Language::Russian) ? "russian" : "english";
}

settings::Language language_from_string(const char* s, settings::Language fallback)
{
    if (std::strcmp(s, "english") == 0) return settings::Language::English;
    if (std::strcmp(s, "russian") == 0) return settings::Language::Russian;
    return fallback;
}

const char* typing_order_to_string(settings::TypingOrder t)
{
    switch (t) {
        case settings::TypingOrder::PasswordOnly:  return "password_only";
        case settings::TypingOrder::PasswordEnter: return "password_enter";
        case settings::TypingOrder::LoginTabPasswordEnter:
        default: return "login_tab_password_enter";
    }
}

settings::TypingOrder typing_order_from_string(const char* s, settings::TypingOrder fallback)
{
    if (std::strcmp(s, "password_only") == 0) return settings::TypingOrder::PasswordOnly;
    if (std::strcmp(s, "password_enter") == 0) return settings::TypingOrder::PasswordEnter;
    if (std::strcmp(s, "login_tab_password_enter") == 0) return settings::TypingOrder::LoginTabPasswordEnter;
    return fallback;
}

const char* wifi_mode_to_string(settings::WifiMode m)
{
    switch (m) {
        case settings::WifiMode::Station:     return "station";
        case settings::WifiMode::AccessPoint: return "access_point";
        case settings::WifiMode::Disabled:
        default: return "disabled";
    }
}

settings::WifiMode wifi_mode_from_string(const char* s, settings::WifiMode fallback)
{
    if (std::strcmp(s, "station") == 0) return settings::WifiMode::Station;
    if (std::strcmp(s, "access_point") == 0) return settings::WifiMode::AccessPoint;
    if (std::strcmp(s, "disabled") == 0) return settings::WifiMode::Disabled;
    return fallback;
}

// -----------------------------------------------------------------
// GET /api/v1/settings
// -----------------------------------------------------------------

cJSON* general_to_json(const settings::GeneralSettings& g)
{
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "language", language_to_string(g.language));
    // Read-only -- see web_settings_routes.hpp's file comment: only
    // one theme value exists right now, nothing to actually select.
    cJSON_AddStringToObject(obj, "theme", "dark");
    cJSON_AddNumberToObject(obj, "display_brightness", g.display_brightness);
    cJSON_AddNumberToObject(obj, "display_off_timeout_s", g.display_off_timeout_s);
    return obj;
}

cJSON* usb_to_json(const settings::UsbSettings& u)
{
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "default_password", u.default_password);
    cJSON_AddStringToObject(obj, "typing_order", typing_order_to_string(u.typing_order));
    cJSON_AddNumberToObject(obj, "delay_before_typing_ms", u.delay_before_typing_ms);
    cJSON_AddNumberToObject(obj, "delay_between_chars_ms", u.delay_between_chars_ms);
    cJSON_AddNumberToObject(obj, "delay_between_fields_ms", u.delay_between_fields_ms);
    cJSON_AddBoolToObject(obj, "cyrillic_auto_switch_layout", u.cyrillic_auto_switch_layout);
    return obj;
}

cJSON* security_to_json(const settings::SecuritySettings& sec)
{
    cJSON* obj = cJSON_CreateObject();
    // pin_length/secret_word deliberately excluded -- see
    // web_settings_routes.hpp's file comment.
    cJSON_AddBoolToObject(obj, "auto_lock_enabled", sec.auto_lock_enabled);
    cJSON_AddNumberToObject(obj, "auto_lock_timeout_s", sec.auto_lock_timeout_s);
    cJSON_AddBoolToObject(obj, "web_ui_view_accounts", (sec.web_ui_permissions & settings::WEB_UI_VIEW_ACCOUNTS) != 0);
    cJSON_AddBoolToObject(obj, "web_ui_edit_accounts", (sec.web_ui_permissions & settings::WEB_UI_EDIT_ACCOUNTS) != 0);
    cJSON_AddBoolToObject(obj, "web_ui_export_data", (sec.web_ui_permissions & settings::WEB_UI_EXPORT_DATA) != 0);
    cJSON_AddBoolToObject(obj, "web_ui_change_settings",
                           (sec.web_ui_permissions & settings::WEB_UI_CHANGE_SETTINGS) != 0);
    return obj;
}

cJSON* wifi_to_json(const settings::WifiSettings& w)
{
    cJSON* obj = cJSON_CreateObject();
    // secret_word excluded here too -- it lives in SecuritySettings,
    // not WifiSettings, and is excluded from BOTH for the same
    // "don't let the web break its own routing" reason.
    cJSON_AddStringToObject(obj, "mode", wifi_mode_to_string(w.mode));
    cJSON_AddStringToObject(obj, "sta_ssid", w.sta_ssid);
    cJSON_AddStringToObject(obj, "sta_password", w.sta_password);
    cJSON_AddStringToObject(obj, "ap_ssid", w.ap_ssid);
    cJSON_AddStringToObject(obj, "ap_password", w.ap_password);
    cJSON_AddBoolToObject(obj, "captive_portal_enabled", w.captive_portal_enabled);
    return obj;
}

esp_err_t handle_get_settings(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    const settings::AllSettings& all = settings::all();
    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "general", general_to_json(all.general));
    cJSON_AddItemToObject(data, "usb", usb_to_json(all.usb));
    cJSON_AddItemToObject(data, "security", security_to_json(all.security));
    cJSON_AddItemToObject(data, "wifi", wifi_to_json(all.wifi));
    respond_ok(req, data);
    return ESP_OK;
}

// -----------------------------------------------------------------
// PUT /api/v1/settings/<section> -- each reads the CURRENT section
// first, then overwrites only the fields present in the JSON body
// (read-modify-write on the whole section, same shape as the
// on-device Settings screens' own Save).
// -----------------------------------------------------------------

esp_err_t handle_put_general(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    std::string body;
    if (!read_body(req, body)) {
        respond_error(req, "400 Bad Request", "Missing or too-large request body");
        return ESP_OK;
    }
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        respond_error(req, "400 Bad Request", "Invalid JSON");
        return ESP_OK;
    }

    settings::GeneralSettings updated = settings::all().general;
    const char* lang_str = json_get_string(root, "language");
    if (lang_str[0] != '\0') {
        updated.language = language_from_string(lang_str, updated.language);
    }
    // theme intentionally not read from the body -- see this file's
    // json output comment.
    updated.display_brightness =
        static_cast<uint8_t>(json_get_uint32(root, "display_brightness", updated.display_brightness));
    if (updated.display_brightness > 100) {
        updated.display_brightness = 100;
    }
    updated.display_off_timeout_s = json_get_uint32(root, "display_off_timeout_s", updated.display_off_timeout_s);
    cJSON_Delete(root);

    if (!settings::set_general(updated)) {
        respond_error(req, "500 Internal Server Error", "Failed to save");
        return ESP_OK;
    }

    // settings::set_general() only persists the value -- it doesn't
    // touch the LIVE display. On-device, GeneralSettingsScreen's own
    // Save doesn't call display::set_brightness() either, but that's
    // invisible there because its brightness ROW already applies
    // changes live as you adjust it (see that screen's "// live
    // preview" comment) -- by the time you hit Save, the display
    // already shows the new value. There's no equivalent live-preview
    // step over the web, so this call is what actually applies the
    // change here instead of leaving the display showing a stale
    // brightness until something else (e.g. next boot, or touching
    // the on-device slider) happens to call this.
    display::set_brightness(updated.display_brightness);

    respond_ok(req, general_to_json(updated));
    return ESP_OK;
}

esp_err_t handle_put_usb(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    std::string body;
    if (!read_body(req, body)) {
        respond_error(req, "400 Bad Request", "Missing or too-large request body");
        return ESP_OK;
    }
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        respond_error(req, "400 Bad Request", "Invalid JSON");
        return ESP_OK;
    }

    settings::UsbSettings updated = settings::all().usb;
    const cJSON* pw_item = cJSON_GetObjectItemCaseSensitive(root, "default_password");
    if (pw_item != nullptr && cJSON_IsString(pw_item) && pw_item->valuestring != nullptr) {
        std::strncpy(updated.default_password, pw_item->valuestring, sizeof(updated.default_password) - 1);
        updated.default_password[sizeof(updated.default_password) - 1] = '\0';
    }
    const char* order_str = json_get_string(root, "typing_order");
    if (order_str[0] != '\0') {
        updated.typing_order = typing_order_from_string(order_str, updated.typing_order);
    }
    updated.delay_before_typing_ms =
        static_cast<uint16_t>(json_get_uint32(root, "delay_before_typing_ms", updated.delay_before_typing_ms));
    updated.delay_between_chars_ms =
        static_cast<uint16_t>(json_get_uint32(root, "delay_between_chars_ms", updated.delay_between_chars_ms));
    updated.delay_between_fields_ms =
        static_cast<uint16_t>(json_get_uint32(root, "delay_between_fields_ms", updated.delay_between_fields_ms));
    const cJSON* auto_switch_item = cJSON_GetObjectItemCaseSensitive(root, "cyrillic_auto_switch_layout");
    if (auto_switch_item != nullptr && cJSON_IsBool(auto_switch_item)) {
        updated.cyrillic_auto_switch_layout = cJSON_IsTrue(auto_switch_item);
    }
    cJSON_Delete(root);

    if (!settings::set_usb(updated)) {
        respond_error(req, "500 Internal Server Error", "Failed to save");
        return ESP_OK;
    }

    respond_ok(req, usb_to_json(updated));
    return ESP_OK;
}

esp_err_t handle_put_security(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    std::string body;
    if (!read_body(req, body)) {
        respond_error(req, "400 Bad Request", "Missing or too-large request body");
        return ESP_OK;
    }
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        respond_error(req, "400 Bad Request", "Invalid JSON");
        return ESP_OK;
    }

    // pin_length/secret_word are NOT read from this body at all --
    // even if a caller sends them, they're ignored. See this file's
    // header comment for why.
    settings::SecuritySettings updated = settings::all().security;

    const cJSON* auto_lock_enabled_item = cJSON_GetObjectItemCaseSensitive(root, "auto_lock_enabled");
    if (auto_lock_enabled_item != nullptr && cJSON_IsBool(auto_lock_enabled_item)) {
        updated.auto_lock_enabled = cJSON_IsTrue(auto_lock_enabled_item);
    }
    updated.auto_lock_timeout_s = json_get_uint32(root, "auto_lock_timeout_s", updated.auto_lock_timeout_s);

    const bool has_any_web_ui_field = cJSON_GetObjectItemCaseSensitive(root, "web_ui_view_accounts") != nullptr ||
                                       cJSON_GetObjectItemCaseSensitive(root, "web_ui_edit_accounts") != nullptr ||
                                       cJSON_GetObjectItemCaseSensitive(root, "web_ui_export_data") != nullptr ||
                                       cJSON_GetObjectItemCaseSensitive(root, "web_ui_change_settings") != nullptr;
    if (has_any_web_ui_field) {
        uint32_t perms = 0;
        if (json_get_bool(root, "web_ui_view_accounts")) perms |= settings::WEB_UI_VIEW_ACCOUNTS;
        if (json_get_bool(root, "web_ui_edit_accounts")) perms |= settings::WEB_UI_EDIT_ACCOUNTS;
        if (json_get_bool(root, "web_ui_export_data")) perms |= settings::WEB_UI_EXPORT_DATA;
        if (json_get_bool(root, "web_ui_change_settings")) perms |= settings::WEB_UI_CHANGE_SETTINGS;
        updated.web_ui_permissions = perms;
    }
    cJSON_Delete(root);

    if (!settings::set_security(updated)) {
        respond_error(req, "500 Internal Server Error", "Failed to save");
        return ESP_OK;
    }

    respond_ok(req, security_to_json(updated));
    return ESP_OK;
}

esp_err_t handle_put_wifi(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    std::string body;
    if (!read_body(req, body)) {
        respond_error(req, "400 Bad Request", "Missing or too-large request body");
        return ESP_OK;
    }
    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        respond_error(req, "400 Bad Request", "Invalid JSON");
        return ESP_OK;
    }

    settings::WifiSettings updated = settings::all().wifi;
    const char* mode_str = json_get_string(root, "mode");
    if (mode_str[0] != '\0') {
        updated.mode = wifi_mode_from_string(mode_str, updated.mode);
    }

    const cJSON* sta_ssid_item = cJSON_GetObjectItemCaseSensitive(root, "sta_ssid");
    if (sta_ssid_item != nullptr && cJSON_IsString(sta_ssid_item) && sta_ssid_item->valuestring != nullptr) {
        std::strncpy(updated.sta_ssid, sta_ssid_item->valuestring, sizeof(updated.sta_ssid) - 1);
        updated.sta_ssid[sizeof(updated.sta_ssid) - 1] = '\0';
    }
    const cJSON* sta_pw_item = cJSON_GetObjectItemCaseSensitive(root, "sta_password");
    if (sta_pw_item != nullptr && cJSON_IsString(sta_pw_item) && sta_pw_item->valuestring != nullptr) {
        std::strncpy(updated.sta_password, sta_pw_item->valuestring, sizeof(updated.sta_password) - 1);
        updated.sta_password[sizeof(updated.sta_password) - 1] = '\0';
    }
    const cJSON* ap_ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ap_ssid");
    if (ap_ssid_item != nullptr && cJSON_IsString(ap_ssid_item) && ap_ssid_item->valuestring != nullptr) {
        std::strncpy(updated.ap_ssid, ap_ssid_item->valuestring, sizeof(updated.ap_ssid) - 1);
        updated.ap_ssid[sizeof(updated.ap_ssid) - 1] = '\0';
    }
    const cJSON* ap_pw_item = cJSON_GetObjectItemCaseSensitive(root, "ap_password");
    if (ap_pw_item != nullptr && cJSON_IsString(ap_pw_item) && ap_pw_item->valuestring != nullptr) {
        std::strncpy(updated.ap_password, ap_pw_item->valuestring, sizeof(updated.ap_password) - 1);
        updated.ap_password[sizeof(updated.ap_password) - 1] = '\0';
    }
    const cJSON* captive_item = cJSON_GetObjectItemCaseSensitive(root, "captive_portal_enabled");
    if (captive_item != nullptr && cJSON_IsBool(captive_item)) {
        updated.captive_portal_enabled = cJSON_IsTrue(captive_item);
    }
    cJSON_Delete(root);

    if (!settings::set_wifi(updated)) {
        respond_error(req, "500 Internal Server Error", "Failed to save");
        return ESP_OK;
    }

    // Matches WifiSettingsScreen's own Save -- the whole point of
    // changing Wi-Fi settings is to reconnect right away.
    ESP_LOGI(TAG, "WiFi settings saved via web, applying...");
    wifi::apply_settings();

    respond_ok(req, wifi_to_json(updated));
    return ESP_OK;
}

} // namespace

void register_settings_routes(httpd_handle_t server)
{
    static char settings_path[64];
    build_prefixed_path(settings_path, sizeof(settings_path), "/api/v1/settings");
    static char general_path[64];
    build_prefixed_path(general_path, sizeof(general_path), "/api/v1/settings/general");
    static char usb_path[64];
    build_prefixed_path(usb_path, sizeof(usb_path), "/api/v1/settings/usb");
    static char security_path[64];
    build_prefixed_path(security_path, sizeof(security_path), "/api/v1/settings/security");
    static char wifi_path[64];
    build_prefixed_path(wifi_path, sizeof(wifi_path), "/api/v1/settings/wifi");

    static httpd_uri_t get_uri{};
    get_uri.uri = settings_path;
    get_uri.method = HTTP_GET;
    get_uri.handler = handle_get_settings;
    get_uri.user_ctx = nullptr;

    static httpd_uri_t put_general_uri{};
    put_general_uri.uri = general_path;
    put_general_uri.method = HTTP_PUT;
    put_general_uri.handler = handle_put_general;
    put_general_uri.user_ctx = nullptr;

    static httpd_uri_t put_usb_uri{};
    put_usb_uri.uri = usb_path;
    put_usb_uri.method = HTTP_PUT;
    put_usb_uri.handler = handle_put_usb;
    put_usb_uri.user_ctx = nullptr;

    static httpd_uri_t put_security_uri{};
    put_security_uri.uri = security_path;
    put_security_uri.method = HTTP_PUT;
    put_security_uri.handler = handle_put_security;
    put_security_uri.user_ctx = nullptr;

    static httpd_uri_t put_wifi_uri{};
    put_wifi_uri.uri = wifi_path;
    put_wifi_uri.method = HTTP_PUT;
    put_wifi_uri.handler = handle_put_wifi;
    put_wifi_uri.user_ctx = nullptr;

    httpd_register_uri_handler(server, &get_uri);
    httpd_register_uri_handler(server, &put_general_uri);
    httpd_register_uri_handler(server, &put_usb_uri);
    httpd_register_uri_handler(server, &put_security_uri);
    httpd_register_uri_handler(server, &put_wifi_uri);

    ESP_LOGI(TAG, "Settings REST routes registered");
}

} // namespace web
