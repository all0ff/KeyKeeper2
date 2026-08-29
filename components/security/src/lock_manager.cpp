#include "security/lock_manager.hpp"

#include "security/session_manager.hpp"

#include "event_bus/event_bus.hpp"
#include "input/input.hpp"
#include "settings/settings.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace security::lock {

namespace {

constexpr char TAG[] = "security.lock";

constexpr uint32_t TASK_POLL_MS = 500;
constexpr uint32_t TASK_STACK_SIZE = 3072;
constexpr uint8_t TASK_PRIORITY = 3;
constexpr size_t MAX_CALLBACKS = 8;

bool initialized = false;
State current_state = State::Locked;

uint32_t last_activity_seen_ms = 0;

struct CallbackSlot
{
    Callback cb = nullptr;
    void* ctx = nullptr;
    bool used = false;
};

CallbackSlot callbacks[MAX_CALLBACKS]{};

TaskHandle_t task_handle = nullptr;
int settings_sub_handle = -1;

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void fire_callbacks(State new_state)
{
    for (const CallbackSlot& slot : callbacks) {
        if (slot.used && slot.cb != nullptr) {
            slot.cb(new_state, slot.ctx);
        }
    }
}

void on_settings_changed(const event_bus::Event& event, void* /*ctx*/)
{
    if (event.category != event_bus::Category::System) {
        return;
    }
    if (event.id != static_cast<uint32_t>(event_bus::SystemEventId::SettingsChanged)) {
        return;
    }
    // We only care if the changed section was Security, but re-reading
    // settings::all() unconditionally on any SettingsChanged is cheap
    // and simpler than decoding the Section payload here -- the values
    // we read (auto_lock_enabled/timeout) just happen to be unchanged
    // if a different section fired.
    ESP_LOGI(TAG, "Settings changed, auto-lock config will be re-read next poll");
}

void auto_lock_task(void* /*arg*/)
{
    const TickType_t period = pdMS_TO_TICKS(TASK_POLL_MS);

    while (true) {
        vTaskDelay(period);

        if (current_state != State::Unlocked) {
            continue;
        }

        const settings::SecuritySettings& sec = settings::all().security;
        if (!sec.auto_lock_enabled) {
            continue;
        }

        const uint32_t activity = input::last_activity_ms();
        const uint32_t idle_for = now_ms() - activity;

        if (idle_for >= sec.auto_lock_timeout_s * 1000u) {
            ESP_LOGI(TAG, "Auto-lock: idle for %u ms, locking", static_cast<unsigned>(idle_for));
            lock();
        }
    }
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!pin::is_initialized() || !session::is_initialized()) {
        ESP_LOGE(TAG, "pin::init() and session::init() must succeed before lock::init()");
        return false;
    }

    current_state = State::Locked; // REQUIREMENTS 9.2: locked at startup
    last_activity_seen_ms = now_ms();

    if (event_bus::is_initialized()) {
        settings_sub_handle =
            event_bus::subscribe(event_bus::Category::System, on_settings_changed, nullptr);
    }

    const BaseType_t task_created = xTaskCreate(
        auto_lock_task, "sec_lock", TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &task_handle);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create auto-lock task");
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "Lock manager initialized (starts Locked)");
    return true;
}

bool is_initialized()
{
    return initialized;
}

State state()
{
    return current_state;
}

pin::VerifyResult unlock(const char* pin_guess)
{
    if (!initialized) {
        return pin::VerifyResult::NoPinSet;
    }

    const pin::VerifyResult result = pin::verify(pin_guess);

    if (result == pin::VerifyResult::Success) {
        current_state = State::Unlocked;
        session::begin_session(session::Origin::Local);

        if (event_bus::is_initialized()) {
            event_bus::publish(event_bus::Category::System,
                                static_cast<uint32_t>(event_bus::SystemEventId::DeviceUnlocked));
        }

        ESP_LOGI(TAG, "Unlocked");
        fire_callbacks(State::Unlocked);
    }

    return result;
}

void lock()
{
    if (!initialized || current_state == State::Locked) {
        return;
    }

    current_state = State::Locked;
    session::end_session();

    if (event_bus::is_initialized()) {
        event_bus::publish(event_bus::Category::System,
                            static_cast<uint32_t>(event_bus::SystemEventId::DeviceLocked));
    }

    ESP_LOGI(TAG, "Locked");
    fire_callbacks(State::Locked);
}

int register_callback(Callback cb, void* ctx)
{
    if (cb == nullptr) {
        return -1;
    }

    for (size_t i = 0; i < MAX_CALLBACKS; ++i) {
        if (!callbacks[i].used) {
            callbacks[i] = {cb, ctx, true};
            return static_cast<int>(i);
        }
    }

    ESP_LOGW(TAG, "Callback registry full (max %u)", static_cast<unsigned>(MAX_CALLBACKS));
    return -1;
}

void unregister_callback(int handle)
{
    if (handle < 0 || static_cast<size_t>(handle) >= MAX_CALLBACKS) {
        return;
    }
    callbacks[handle].used = false;
}

} // namespace security::lock
