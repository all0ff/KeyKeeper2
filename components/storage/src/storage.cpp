#include "storage/storage.hpp"

#include "storage/filesystem.hpp"
#include "storage/nvs_storage.hpp"
#include "storage/sdcard.hpp"

#include "esp_log.h"

namespace storage {

namespace {

constexpr char TAG[] = "storage";

bool initialized = false;
Status current_status{false, false};

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!nvs::init()) {
        ESP_LOGE(TAG, "NVS init failed");
        return false;
    }

    if (!fs::init()) {
        ESP_LOGE(TAG, "Internal LittleFS init failed -- vault cannot function, halting storage init");
        return false;
    }
    current_status.littlefs_ready = true;

    // Not fatal if this comes back false -- the card is optional.
    current_status.sdcard_present = sd::init();
    if (!current_status.sdcard_present) {
        ESP_LOGI(TAG, "No SD card mounted -- backup/import/export unavailable until one is inserted");
    }

    initialized = true;
    ESP_LOGI(TAG, "Storage initialized (littlefs=ready, sdcard=%s)",
             current_status.sdcard_present ? "present" : "absent");

    return true;
}

bool is_initialized()
{
    return initialized;
}

const Status& status()
{
    return current_status;
}

bool refresh_sdcard()
{
    current_status.sdcard_present = sd::remount();
    return current_status.sdcard_present;
}

} // namespace storage
