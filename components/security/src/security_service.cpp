#include "security/security_service.hpp"

#include "security/lock_manager.hpp"
#include "security/permission_manager.hpp"
#include "security/pin_manager.hpp"
#include "security/session_manager.hpp"

#include "esp_log.h"

namespace security {

namespace {

constexpr char TAG[] = "security";

bool initialized = false;

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!pin::init()) {
        ESP_LOGE(TAG, "pin::init() failed");
        return false;
    }

    if (!session::init()) {
        ESP_LOGE(TAG, "session::init() failed");
        return false;
    }

    if (!lock::init()) {
        ESP_LOGE(TAG, "lock::init() failed");
        return false;
    }

    if (!permission::init()) {
        ESP_LOGE(TAG, "permission::init() failed");
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "Security initialized (pin_set=%d, state=%s)",
             pin::has_pin(),
             lock::state() == lock::State::Locked ? "Locked" : "Unlocked");

    return true;
}

bool is_initialized()
{
    return initialized;
}

} // namespace security
