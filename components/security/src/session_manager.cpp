#include "security/session_manager.hpp"

#include "esp_log.h"
#include "esp_timer.h"

namespace security::session {

namespace {

constexpr char TAG[] = "security.session";

bool initialized = false;
Session current_session{};

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    current_session = Session{};
    initialized = true;
    return true;
}

bool is_initialized()
{
    return initialized;
}

bool has_active_session()
{
    return current_session.active;
}

const Session& current()
{
    return current_session;
}

void begin_session(Origin origin)
{
    current_session.active = true;
    current_session.origin = origin;
    current_session.started_ms = now_ms();
    ESP_LOGI(TAG, "Session started (origin=%d)", static_cast<int>(origin));
}

void end_session()
{
    if (current_session.active) {
        ESP_LOGI(TAG, "Session ended");
    }
    current_session = Session{};
}

} // namespace security::session
