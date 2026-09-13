#include "web/web_service.hpp"

#include "event_bus/event_bus.hpp"
#include "security/lock_manager.hpp"
#include "security/pin_manager.hpp"
#include "settings/settings.hpp"
#include "wifi/wifi_service.hpp"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include <algorithm>
#include <cstring>

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

// -----------------------------------------------------------------
// JSON response helpers -- {"status":"ok","data":{...}} /
// {"status":"error","message":"..."}, per docs/WEB.md section 7.
// -----------------------------------------------------------------

void respond_ok(httpd_req_t* req, cJSON* data)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddItemToObject(root, "data", (data != nullptr) ? data : cJSON_CreateObject());

    char* text = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
    cJSON_free(text);
    cJSON_Delete(root);
}

void respond_error(httpd_req_t* req, const char* http_status_line, const char* message)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddStringToObject(root, "message", message);

    char* text = cJSON_PrintUnformatted(root);
    httpd_resp_set_status(req, http_status_line);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
    cJSON_free(text);
    cJSON_Delete(root);
}

// -----------------------------------------------------------------
// Minimal login page -- just enough to exercise the login endpoint
// from a browser without needing curl/Postman. Not the real Web UI
// (see web_service.hpp's file comment) -- that's a separate,
// later increment.
// -----------------------------------------------------------------

constexpr char LOGIN_PAGE[] = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KeyKeeper2</title>
<style>
  body { font-family: sans-serif; max-width: 320px; margin: 48px auto; padding: 0 16px; }
  input, button { width: 100%; padding: 10px; margin: 8px 0; font-size: 1rem; box-sizing: border-box; }
  #msg { min-height: 1.4em; font-size: 0.9rem; }
</style>
</head>
<body>
  <h2>KeyKeeper2</h2>
  <p id="msg"></p>
  <input id="pin" type="password" inputmode="numeric" placeholder="PIN" autofocus>
  <button onclick="login()">Unlock</button>
  <script>
    async function login() {
      const pin = document.getElementById('pin').value;
      const msg = document.getElementById('msg');
      msg.style.color = '#000';
      msg.textContent = 'Checking...';
      try {
        const res = await fetch('/api/v1/auth/login', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ pin: pin })
        });
        const body = await res.json();
        if (res.ok) {
          msg.style.color = '#080';
          msg.textContent = 'Unlocked.';
        } else {
          msg.style.color = '#c00';
          msg.textContent = body.message || ('Error ' + res.status);
        }
      } catch (e) {
        msg.style.color = '#c00';
        msg.textContent = 'Request failed: ' + e;
      }
    }
  </script>
</body>
</html>
)HTML";

esp_err_t handle_root(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, LOGIN_PAGE, HTTPD_RESP_USE_STRLEN);
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

    // security::lock::unlock() -- the SAME call LockScreen makes.
    // See web_service.hpp's "SHARED SESSION MODEL" comment for what
    // this means and doesn't mean.
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
            // Deliberately NOT the same response as
            // ui::screens::LockScreen's WipeRequired case. A remote
            // brute-force attempt over the network doesn't need a
            // destructive, irreversible response the way repeated
            // physical-device guesses might -- disabling Wi-Fi cuts
            // off the remote attack surface entirely (reversibly: the
            // owner can re-enable it from WifiSettingsScreen) without
            // touching the vault or the PIN at all. This still shares
            // pin_manager's single failure counter with on-device
            // attempts -- whichever channel happens to receive the
            // 12th failure decides the outcome (wipe if on-device,
            // Wi-Fi disabled if via this endpoint).
            ESP_LOGW(TAG, "PIN failure threshold reached via web login -- disabling WiFi instead of wiping");

            settings::WifiSettings disabled = settings::all().wifi;
            disabled.mode = settings::WifiMode::Disabled;
            settings::set_wifi(disabled); // persisted -- stays off across reboots until re-enabled on-device
            wifi::apply_settings();       // stop the radio right away, don't wait for a reboot

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

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start() failed");
        server = nullptr;
        return false;
    }

    static httpd_uri_t root_uri{};
    root_uri.uri = "/";
    root_uri.method = HTTP_GET;
    root_uri.handler = handle_root;
    root_uri.user_ctx = nullptr;

    static httpd_uri_t login_uri{};
    login_uri.uri = "/api/v1/auth/login";
    login_uri.method = HTTP_POST;
    login_uri.handler = handle_login;
    login_uri.user_ctx = nullptr;

    static httpd_uri_t status_uri{};
    status_uri.uri = "/api/v1/auth/status";
    status_uri.method = HTTP_GET;
    status_uri.handler = handle_auth_status;
    status_uri.user_ctx = nullptr;

    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &login_uri);
    httpd_register_uri_handler(server, &status_uri);

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

} // namespace web
