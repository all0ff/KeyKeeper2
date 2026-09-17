#include "rtc_time/rtc_time.hpp"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

#include <ctime>

namespace rtc_time {

namespace {

constexpr char TAG[] = "rtc_time";

// A single, well-known public pool -- no project-specific reason to
// prefer any particular server. Revisit if this ever needs to work
// somewhere pool.ntp.org is blocked/unreachable.
constexpr char NTP_SERVER[] = "pool.ntp.org";

bool initialized = false;
bool synced = false;

void sntp_sync_event_handler(void* /*arg*/, esp_event_base_t /*base*/, int32_t /*id*/, void* /*data*/)
{
    synced = true;
    ESP_LOGI(TAG, "Time synchronized via NTP");
}

} // namespace

bool init()
{
    if (initialized) {
        return true;
    }

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    // Don't auto-start at init() -- there's no network yet at boot.
    // wifi:: explicitly calls start_sync() once Station actually has
    // an IP (see that component's IP_EVENT_STA_GOT_IP handler).
    config.start = false;

    if (esp_netif_sntp_init(&config) != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_sntp_init() failed");
        return false;
    }

    // NETIF_SNTP_EVENT/NETIF_SNTP_TIME_SYNC -- confirmed against
    // ESP-IDF's own esp_netif_sntp.h for this project's v6.0.2;
    // please double check this compiles as expected on first build,
    // this API has shifted across ESP-IDF versions before.
    const esp_err_t err =
        esp_event_handler_register(NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC, &sntp_sync_event_handler, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register SNTP sync event handler");
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "SNTP client initialized (server: %s)", NTP_SERVER);
    return true;
}

bool is_initialized()
{
    return initialized;
}

void start_sync()
{
    if (!initialized) {
        return;
    }
    // "Start SNTP service if it wasn't started during init, or
    // restart it if already started" -- per esp_netif_sntp.h's own
    // doc comment, safe to call repeatedly.
    esp_netif_sntp_start();
    ESP_LOGI(TAG, "SNTP sync requested");
}

bool is_synced()
{
    return synced;
}

uint64_t unix_time()
{
    return static_cast<uint64_t>(std::time(nullptr));
}

} // namespace rtc_time
