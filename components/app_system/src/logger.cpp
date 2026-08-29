#include "app_system/logger.hpp"

#include "esp_log.h"

namespace app_system::logger {

namespace {

constexpr char TAG[] = "system";

bool g_initialized = false;

} // namespace

bool init()
{
    if (g_initialized) {
        return true;
    }

    g_initialized = true;

    ESP_LOGI(TAG, "System logger initialized");
    return true;
}

bool is_initialized()
{
    return g_initialized;
}

void boot_stage(const char* name)
{
    if (name == nullptr) {
        ESP_LOGI(TAG, "Boot stage started");
        return;
    }

    ESP_LOGI(TAG, "Boot stage: %s", name);
}

void ready()
{
    ESP_LOGI(TAG, "KeyKeeper2 application is ready");
}

void error(const char* message)
{
    if (message == nullptr) {
        ESP_LOGE(TAG, "Application error");
        return;
    }

    ESP_LOGE(TAG, "%s", message);
}

void info(const char* message)
{
    if (message == nullptr) {
        ESP_LOGI(TAG, "Application info");
        return;
    }

    ESP_LOGI(TAG, "%s", message);
}

void state(uint8_t runtime_state, bool ready_state, uint32_t error_flags)
{
    ESP_LOGI(
        TAG,
        "runtime_state=%u ready=%d error_flags=0x%08lx",
        static_cast<unsigned>(runtime_state),
        ready_state ? 1 : 0,
        static_cast<unsigned long>(error_flags)
    );
}

} // namespace app_system::logger