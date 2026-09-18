#include "web/web_service.hpp"

#include "event_bus/event_bus.hpp"
#include "security/lock_manager.hpp"
#include "security/pin_manager.hpp"
#include "settings/settings.hpp"
#include "wifi/wifi_service.hpp"
#include "web_app_html.hpp"
#include "web_json_helpers.hpp"
#include "web_russian_localization.hpp"
#include "web_settings_routes.hpp"
#include "web_vault_routes.hpp"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace web {

namespace {

constexpr char TAG[] = "web";

bool initialized = false;
httpd_handle_t server = nullptr;

void publish(WebEventId id)
{
    if (!event_bus::is_initialized()) {
        return;
    }
    event_bus::publish(event_bus::Category::Web, static_cast<uint32_t>(id));
}

esp_err_t handle_root(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    // no-store -- without this, a browser can (and evidently did, in
    // practice) keep serving an OLD cached copy of this page after a
    // firmware update changed it, with no visible sign anything was
    // wrong (no error, just silently stale content -- new features
    // looking like they were never applied at all). This page is
    // generated fresh from the running firmware on every request
    // anyway (APP_PAGE is a compiled-in constant, not a file read),
    // so there's no cost to never caching it.
    //
    // The localization script is deliberately injected here rather
    // than changing the embedded HTML itself. It reads the persisted
    // language through the existing settings API and translates only
    // user-visible text. API paths, field names and JavaScript logic
    // therefore remain unchanged.
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    static const std::string page = [] {
        std::string result(APP_PAGE);
        const std::string marker = "</body>";
        const size_t pos = result.find(marker);
        if (pos != std::string::npos) {
            result.insert(pos, RUSSIAN_LOCALIZATION_SCRIPT);
        }
        return result;
    }();

    httpd_resp_send(req, page.c_str(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t handle_auth_status(httpd_req_t* req)
{
    cJSON* data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "authenticated",
                           security::lock::state() == security::lock::State::Unlocked);
    respond_ok(req, data);
    return ESP_OK;
}

esp_err_t handle_login(httpd_req_t* req)
{
    char buf[128];
    const size_t recv_size = std::min(static_cast<size_t>(req->content_len), sizeof(buf) - 1);

    const int ret = httpd_req_recv(req, buf, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    const cJSON* pin_item = (root != nullptr) ? cJSON_GetObjectItemCaseSensitive(root, "pin") : nullptr;

    if (pin_item == nullptr || !cJSON_IsString(pin_item) || pin_item->valuestring == nullptr) {
        if (root != nullptr) {
            cJSON_Delete(root);
        }
        respond_error(req, "400 Bad Request", "Missing 'pin' field");
        return ESP_OK;
    }

    const security::pin::VerifyResult result = security::lock::unlock(pin_item->valuestring);
    cJSON_Delete(root);

    switch (result) {
        case security::pin::VerifyResult::Success: {
            ESP_LOGI(TAG, "Web login succeeded");
            publish(WebEventId::LoginSucceeded);
            cJSON* data = cJSON_CreateObject();
            cJSON_AddBoolToObject(data, "authenticated", true);
            respond_ok(req, data);
            return ESP_OK;
        }

        case security::pin::VerifyResult::WipeRequired: {
            ESP_LOGW(TAG, "PIN failure threshold reached via web login -- disabling WiFi instead of wiping");

            settings::WifiSettings disabled = settings::all().wifi;
            disabled.mode = settings::WifiMode::Disabled;
            settings::set_wifi(disabled);
            wifi::apply_settings();

            respond_error(req, "403 Forbidden", "Too many failed attempts -- WiFi disabled");
            return ESP_OK;
        }

        case security::pin::VerifyResult::LockedOut:
            publish(WebEventId::LoginFailed);
            respond_error(req, "423 Locked", "Locked out, try again later");
            return ESP_OK;

        case security::pin::VerifyResult::NoPinSet:
            respond_error(req, "409 Conflict", "No PIN configured yet -- set one up on the device first");
            return ESP_OK;

        case security::pin::VerifyResult::WrongPin:
        default:
            publish(WebEventId::LoginFailed);
            respond_error(req, "401 Unauthorized", "Wrong PIN");
            return ESP_OK;
    }
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }
    initialized = true;
    ESP_LOGI(TAG, "WebService initialized (server not started yet -- call start())");
    return true;
}

bool is_initialized()
{
    return initialized;
}

bool start()
{
    if (!initialized) {
        return false;
    }
    if (server != nullptr) {
        ESP_LOGW(TAG, "start() called while already running, ignoring");
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 24;
    config.stack_size = 8192;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start() failed");
        server = nullptr;
        return false;
    }

    static httpd_uri_t root_uri{};
    static char root_path[48];
    build_prefixed_path(root_path, sizeof(root_path), "/");
    root_uri.uri = root_path;
    root_uri.method = HTTP_GET;
    root_uri.handler = handle_root;
    root_uri.user_ctx = nullptr;

    static httpd_uri_t login_uri{};
    static char login_path[64];
    build_prefixed_path(login_path, sizeof(login_path), "/api/v1/auth/login");
    login_uri.uri = login_path;
    login_uri.method = HTTP_POST;
    login_uri.handler = handle_login;
    login_uri.user_ctx = nullptr;

    static httpd_uri_t status_uri{};
    static char status_path[64];
    build_prefixed_path(status_path, sizeof(status_path), "/api/v1/auth/status");
    status_uri.uri = status_path;
    status_uri.method = HTTP_GET;
    status_uri.handler = handle_auth_status;
    status_uri.user_ctx = nullptr;

    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &login_uri);
    httpd_register_uri_handler(server, &status_uri);

    register_vault_routes(server);
    register_settings_routes(server);

    ESP_LOGI(TAG, "HTTP server started");
    publish(WebEventId::ServerStarted);
    return true;
}

void stop()
{
    if (server == nullptr) {
        return;
    }
    httpd_stop(server);
    server = nullptr;
    ESP_LOGI(TAG, "HTTP server stopped");
    publish(WebEventId::ServerStopped);
}

bool is_running()
{
    return server != nullptr;
}

bool restart()
{
    stop();
    return start();
}

} // namespace web
