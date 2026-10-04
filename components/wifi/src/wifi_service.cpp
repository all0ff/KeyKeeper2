#include "wifi/wifi_service.hpp"

#include "event_bus/event_bus.hpp"
#include "rtc_time/rtc_time.hpp"
#include "settings/settings.hpp"
#include "wifi/captive_dns.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

#include <cstdio>
#include <cstring>

namespace wifi {

namespace {

constexpr char TAG[] = "wifi";

// How many times to retry a Station connection attempt before giving
// up and reporting ConnectionFailed -- REQUIREMENTS/07_wifi.md don't
// specify a number, this is a reasonable placeholder, not a
// considered policy.
constexpr uint8_t MAX_STA_RETRIES = 5;

bool initialized = false;

// current_state/last_error_buf/sta_retry_count/ap_client_count_value
// below (audit finding Q-04) are written from handle_wifi_event()/
// handle_ip_event() -- ESP-IDF event-loop callbacks, which run on the
// system event task -- and read from the public getters
// (state()/ap_client_count()/last_error()/ip_address()), called from
// whichever task the UI/web layer happens to be on. Genuinely
// different tasks, no synchronization before this.
//
// A spinlock (portMUX_TYPE), not per-field std::atomic: these four
// fields are updated TOGETHER as one logical transition in most of
// the call sites below (e.g. "entering Connecting" always resets
// sta_retry_count in the same breath) -- independent atomics on each
// field would make every individual read/write safe on its own while
// still letting a reader observe a torn COMBINATION (the new state
// with the old retry count, say). The whole block needs one critical
// section, not four separate ones. portMUX_TYPE specifically (over a
// FreeRTOS mutex/semaphore) because it's safe to initialize right
// here, at static-init time, before the scheduler is running --
// xSemaphoreCreateMutex() is not -- and every critical section below
// is short enough (plain field reads/writes, no blocking calls
// inside) that a spinlock is the correct tool, not just a convenient
// one.
portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;

ConnectionState current_state = ConnectionState::Idle;
char last_error_buf[64] = "";
uint8_t sta_retry_count = 0;
uint8_t ap_client_count_value = 0;

esp_netif_t* sta_netif = nullptr;
esp_netif_t* ap_netif = nullptr;
esp_event_handler_instance_t wifi_event_instance = nullptr;
esp_event_handler_instance_t ip_event_instance = nullptr;

// Caller already holds state_lock -- a plain helper, not itself
// locking, to avoid the non-recursive-spinlock deadlock a nested
// portENTER_CRITICAL() would cause from inside handle_wifi_event()'s
// own critical section.
void set_last_error_locked(const char* message)
{
    std::strncpy(last_error_buf, message, sizeof(last_error_buf) - 1);
    last_error_buf[sizeof(last_error_buf) - 1] = '\0';
}

void set_last_error(const char* message)
{
    portENTER_CRITICAL(&state_lock);
    set_last_error_locked(message);
    portEXIT_CRITICAL(&state_lock);
}

void publish(WifiEventId id, uint32_t payload_u32 = 0)
{
    if (!event_bus::is_initialized()) {
        return;
    }
    event_bus::Payload payload{};
    payload.u32 = payload_u32;
    event_bus::publish(event_bus::Category::Wifi, static_cast<uint32_t>(id), payload);
}

// A handful of common Wi-Fi disconnect reasons, mapped to something a
// person can actually understand -- not exhaustive (wifi_err_reason_t
// has dozens of values), just the ones worth naming specifically.
const char* describe_disconnect_reason(uint8_t reason)
{
    switch (reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "wrong password";
        case WIFI_REASON_NO_AP_FOUND:
            return "network not found";
        case WIFI_REASON_ASSOC_LEAVE:
            return "disconnected";
        default:
            return "connection failed";
    }
}

void handle_wifi_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    (void)arg;
    (void)event_base;

    switch (event_id) {
        case WIFI_EVENT_STA_START: {
            portENTER_CRITICAL(&state_lock);
            sta_retry_count = 0;
            current_state = ConnectionState::Connecting;
            portEXIT_CRITICAL(&state_lock);
            publish(WifiEventId::Connecting);
            esp_wifi_connect();
            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED: {
            const auto* info = static_cast<wifi_event_sta_disconnected_t*>(event_data);
            const uint8_t reason = (info != nullptr) ? info->reason : 0;

            publish(WifiEventId::Disconnected);

            portENTER_CRITICAL(&state_lock);
            const bool will_retry = sta_retry_count < MAX_STA_RETRIES;
            if (will_retry) {
                ++sta_retry_count;
                current_state = ConnectionState::Connecting;
            } else {
                current_state = ConnectionState::Failed;
                set_last_error_locked(describe_disconnect_reason(reason));
            }
            const uint8_t retry_count_now = sta_retry_count;
            portEXIT_CRITICAL(&state_lock);

            if (will_retry) {
                ESP_LOGW(TAG, "STA disconnected (reason %u), retry %u/%u", static_cast<unsigned>(reason),
                         static_cast<unsigned>(retry_count_now), static_cast<unsigned>(MAX_STA_RETRIES));
                esp_wifi_connect();
            } else {
                ESP_LOGE(TAG, "STA connection failed: %s", last_error_buf);
                publish(WifiEventId::ConnectionFailed);
            }
            break;
        }

        case WIFI_EVENT_AP_STACONNECTED: {
            portENTER_CRITICAL(&state_lock);
            const uint8_t count = ++ap_client_count_value;
            portEXIT_CRITICAL(&state_lock);
            publish(WifiEventId::ApClientJoined, count);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            portENTER_CRITICAL(&state_lock);
            if (ap_client_count_value > 0) {
                --ap_client_count_value;
            }
            const uint8_t count = ap_client_count_value;
            portEXIT_CRITICAL(&state_lock);
            publish(WifiEventId::ApClientLeft, count);
            break;
        }

        default:
            break;
    }
}

void handle_ip_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;

    if (event_id == IP_EVENT_STA_GOT_IP) {
        portENTER_CRITICAL(&state_lock);
        sta_retry_count = 0;
        current_state = ConnectionState::Connected;
        last_error_buf[0] = '\0';
        portEXIT_CRITICAL(&state_lock);
        publish(WifiEventId::Connected);

        // Only real clock source on this board -- see rtc_time.hpp's
        // own file comment. Safe/cheap to call on every reconnect,
        // not just the first one (also helps catch clock drift on a
        // long-running session).
        rtc_time::start_sync();
    }
}

bool start_station(const settings::WifiSettings& cfg)
{
    wifi_config_t wifi_config{};
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), cfg.sta_ssid, sizeof(wifi_config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password), cfg.sta_password,
                 sizeof(wifi_config.sta.password) - 1);

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode(STA) failed");
        return false;
    }
    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(STA) failed");
        return false;
    }
    if (esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start() failed for STA");
        return false;
    }

    // Actual connect() happens in WIFI_EVENT_STA_START's handler.
    return true;
}

bool start_access_point(const settings::WifiSettings& cfg)
{
    const size_t password_len = std::strlen(cfg.ap_password);
    if (password_len > 0 && password_len < 8) {
        // ESP-IDF's WPA2-PSK requires an 8+ character passphrase --
        // esp_wifi_start() would otherwise fail with
        // ESP_ERR_WIFI_PASSWORD. Reject up front with a clearer log
        // than that error code gives on its own.
        ESP_LOGE(TAG, "AP password must be empty (open network) or at least 8 characters");
        set_last_error("AP password too short (min 8 chars)");
        return false;
    }

    wifi_config_t wifi_config{};
    std::strncpy(reinterpret_cast<char*>(wifi_config.ap.ssid), cfg.ap_ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = static_cast<uint8_t>(std::strlen(cfg.ap_ssid));
    std::strncpy(reinterpret_cast<char*>(wifi_config.ap.password), cfg.ap_password,
                 sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.max_connection = 4; // placeholder, not spec'd anywhere
    wifi_config.ap.authmode = (password_len == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode(AP) failed");
        return false;
    }
    if (esp_wifi_set_config(WIFI_IF_AP, &wifi_config) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(AP) failed");
        return false;
    }
    if (esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start() failed for AP");
        return false;
    }

    portENTER_CRITICAL(&state_lock);
    ap_client_count_value = 0;
    current_state = ConnectionState::ApRunning;
    portEXIT_CRITICAL(&state_lock);
    publish(WifiEventId::ApStarted);

#if CONFIG_KEYKEEPER_LITE
    // This is the ACTUAL enforcement point for the Lite strip, not
    // just the UI (wifi_settings_screen.cpp's own CaptivePortal row,
    // forced to always show/act as off) -- cfg.captive_portal_enabled
    // below reads straight from stored settings::WifiSettings, which
    // a device previously Full with this ON, then reflashed to Lite,
    // would still carry as true in existing NVS (settings don't get
    // reset by reflashing, and that screen's own stale-NVS guard only
    // stops the UI from ever WRITING true again -- it doesn't retroactively
    // fix what's already stored, which this check would otherwise
    // still faithfully honor). Forced false here regardless of what's
    // actually stored -- without this, the UI would show "Off" while
    // captive_dns::start() below still silently ran, the opposite of
    // what Lite is supposed to guarantee.
    constexpr bool captive_portal_enabled = false;
#else
    const bool captive_portal_enabled = cfg.captive_portal_enabled;
#endif
    if (!captive_portal_enabled) {
        ESP_LOGI(TAG, "Captive portal disabled in settings -- AP running as a plain access point");
        return true;
    }

    // Makes the AP self-explanatory to connect to -- see
    // captive_dns.hpp's own file comment for the full mechanism (DNS
    // hijack here + web_service.cpp's wildcard HTTP redirect). Not a
    // hard failure if this doesn't start -- the AP and its own login
    // page still work fine via a manually-typed IP either way, this
    // is a convenience layer on top, not a dependency.
    if (!captive_dns::start(ap_netif)) {
        ESP_LOGW(TAG, "Captive DNS failed to start -- AP still usable via manual IP entry");
    }

    return true;
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!settings::is_initialized()) {
        ESP_LOGE(TAG, "settings::init() must succeed before wifi::init()");
        return false;
    }

    if (esp_netif_init() != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init() failed");
        return false;
    }
    if (esp_event_loop_create_default() != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_loop_create_default() failed");
        return false;
    }

    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();
    if (sta_netif == nullptr || ap_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create default Wi-Fi netifs");
        return false;
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wifi_init_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init() failed");
        return false;
    }

    if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &handle_wifi_event, nullptr,
                                             &wifi_event_instance) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WIFI_EVENT handler");
        return false;
    }
    if (esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handle_ip_event, nullptr,
                                             &ip_event_instance) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP_EVENT handler");
        return false;
    }

    initialized = true;
    portENTER_CRITICAL(&state_lock);
    current_state = ConnectionState::Idle;
    portEXIT_CRITICAL(&state_lock);
    ESP_LOGI(TAG, "WiFiService initialized (radio not started yet -- call apply_settings())");
    publish(WifiEventId::Started);
    return true;
}

bool is_initialized()
{
    return initialized;
}

bool apply_settings()
{
    if (!initialized) {
        return false;
    }

    // Tear down whatever was running before applying a (possibly
    // different) mode -- safe to call even if the radio isn't
    // currently started. Captive DNS specifically must never keep
    // running past AP mode itself -- hijacking DNS while Station mode
    // is connected to a real network would break normal browsing on
    // it, not just be pointless.
    captive_dns::stop();
    esp_wifi_stop();
    portENTER_CRITICAL(&state_lock);
    sta_retry_count = 0;
    ap_client_count_value = 0;
    last_error_buf[0] = '\0';
    portEXIT_CRITICAL(&state_lock);

    const settings::WifiSettings& cfg = settings::all().wifi;

    switch (cfg.mode) {
        case settings::WifiMode::Disabled: {
            esp_wifi_set_mode(WIFI_MODE_NULL);
            portENTER_CRITICAL(&state_lock);
            current_state = ConnectionState::Idle;
            portEXIT_CRITICAL(&state_lock);
            publish(WifiEventId::Stopped);
            return true;
        }

        case settings::WifiMode::Station: {
            portENTER_CRITICAL(&state_lock);
            current_state = ConnectionState::Connecting;
            portEXIT_CRITICAL(&state_lock);
            return start_station(cfg);
        }

        case settings::WifiMode::AccessPoint:
            return start_access_point(cfg);
    }

    return false;
}

void stop()
{
    if (!initialized) {
        return;
    }
    captive_dns::stop();
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    portENTER_CRITICAL(&state_lock);
    current_state = ConnectionState::Idle;
    portEXIT_CRITICAL(&state_lock);
    publish(WifiEventId::Stopped);
}

ConnectionState state()
{
    portENTER_CRITICAL(&state_lock);
    const ConnectionState snapshot = current_state;
    portEXIT_CRITICAL(&state_lock);
    return snapshot;
}

const char* ip_address()
{
    static char buf[16];
    buf[0] = '\0';

    // One snapshot of current_state for both decisions below (which
    // netif, and whether to bother at all) -- re-reading the shared
    // field separately for each would risk acting on two different
    // moments in time if a transition happens to land exactly between
    // them. The netif pointers themselves aren't behind this lock
    // (sta_netif/ap_netif are set once in init(), long before any
    // concurrent access is possible, and never reassigned after), so
    // only the state read itself needs the critical section.
    portENTER_CRITICAL(&state_lock);
    const ConnectionState snapshot = current_state;
    portEXIT_CRITICAL(&state_lock);

    if (snapshot != ConnectionState::Connected && snapshot != ConnectionState::ApRunning) {
        return buf;
    }

    esp_netif_t* netif = (snapshot == ConnectionState::ApRunning) ? ap_netif : sta_netif;
    if (netif == nullptr) {
        return buf;
    }

    esp_netif_ip_info_t ip_info{};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return buf;
    }

    std::snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ip_info.ip));
    return buf;
}

uint8_t ap_client_count()
{
    portENTER_CRITICAL(&state_lock);
    const uint8_t count = (current_state == ConnectionState::ApRunning) ? ap_client_count_value : 0;
    portEXIT_CRITICAL(&state_lock);
    return count;
}

const char* last_error()
{
    // KNOWN RESIDUAL LIMITATION: this still returns a raw pointer
    // into the shared last_error_buf, not a copy -- the critical
    // section here only protects the pointer READ itself from racing
    // a concurrent WRITE to current_state/the other fields; it does
    // NOT protect whatever the caller does with the returned pointer
    // afterward, since last_error_buf's own bytes could still be
    // overwritten by a later event while the caller is mid-read.
    // Fully closing that would mean changing this into an output-
    // buffer-style API (last_error(char* out, size_t out_size), copy
    // under the lock) -- not done here since it has exactly one
    // caller today (wifi_settings_screen.cpp, which copies the
    // content out immediately via lv_label_set_text_fmt() -- a brief,
    // low-risk window in practice, not a long-held reference), and
    // changing the public signature for that one call site felt like
    // a bigger, separate change from "add the missing synchronization"
    // -- worth doing explicitly if this component grows more callers.
    portENTER_CRITICAL(&state_lock);
    const char* result = last_error_buf;
    portEXIT_CRITICAL(&state_lock);
    return result;
}

} // namespace wifi
