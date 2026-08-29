#include "security/permission_manager.hpp"

#include "security/lock_manager.hpp"
#include "security/session_manager.hpp"

#include "settings/settings.hpp"

#include "esp_log.h"

namespace security::permission {

namespace {

constexpr char TAG[] = "security.permission";

bool initialized = false;

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!lock::is_initialized() || !session::is_initialized()) {
        ESP_LOGE(TAG, "lock::init() and session::init() must succeed before permission::init()");
        return false;
    }

    initialized = true;
    return true;
}

bool is_initialized()
{
    return initialized;
}

Result check(Operation op)
{
    if (!initialized) {
        return Result::DeniedLocked;
    }

    if (lock::state() != lock::State::Unlocked) {
        return Result::DeniedLocked;
    }

    if (!session::has_active_session()) {
        return Result::DeniedNoSession;
    }

    if (op == Operation::WebLogin) {
        if (settings::all().security.web_ui_permissions == settings::WEB_UI_NONE) {
            return Result::DeniedWebUiDisabled;
        }
    }

    return Result::Allowed;
}

bool is_allowed(Operation op)
{
    return check(op) == Result::Allowed;
}

} // namespace security::permission
