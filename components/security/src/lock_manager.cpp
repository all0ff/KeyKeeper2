#include "security/lock_manager.hpp"

#include "security/session_manager.hpp"
#include "security/vault_key.hpp"

#include "event_bus/event_bus.hpp"
#include "input/input.hpp"
#include "settings/settings.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>

namespace security::lock {

namespace {

constexpr char TAG[] = "security.lock";

constexpr uint32_t TASK_POLL_MS = 500;
constexpr uint32_t TASK_STACK_SIZE = 3072;
constexpr uint8_t TASK_PRIORITY = 3;
constexpr size_t MAX_CALLBACKS = 8;

bool initialized = false;
State current_state = State::Locked;
// See notify_activity()'s own comment -- tracked separately from
// input::last_activity_ms() (security:: has no business writing to
// input::'s own state) and combined with it in auto_lock_task()'s own
// idle calculation below.
uint32_t last_notified_activity_ms = 0;

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

        // Combined with last_notified_activity_ms (see
        // notify_activity()'s own comment) -- either source counts as
        // "the system is actively being used" for auto-lock purposes,
        // not just physical input.
        const uint32_t activity = std::max(input::last_activity_ms(), last_notified_activity_ms);
        const uint32_t idle_for = now_ms() - activity;

        if (idle_for >= sec.auto_lock_timeout_s * 1000u) {
            ESP_LOGI(TAG, "Auto-lock: idle for %u ms, locking", static_cast<unsigned>(idle_for));
            lock();
        }
    }
}

void transition_to_unlocked()
{
    current_state = State::Unlocked;
    session::begin_session(session::Origin::Local);
    // Establishes a fresh baseline right at the moment of unlock --
    // without this, a web-based unlock (which doesn't touch
    // input::last_activity_ms() at all) could in principle still read
    // as instantly idle on auto_lock_task()'s very next poll if
    // nothing else happened to call notify_activity() first (a
    // request to any require_unlocked() route already does, per that
    // function's own comment, but this closes the gap unconditionally
    // rather than depending on one arriving in time).
    notify_activity();

    if (event_bus::is_initialized()) {
        event_bus::publish(event_bus::Category::System,
                            static_cast<uint32_t>(event_bus::SystemEventId::DeviceUnlocked));
    }

    ESP_LOGI(TAG, "Unlocked");
    fire_callbacks(State::Unlocked);
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

    // Duress check runs FIRST, before the regular one. It is a fast
    // salted SHA-256 pass, not PBKDF2, because the duress PIN is only
    // a trigger for the duress action and does not protect the vault.
    // Therefore, when a duress PIN is configured, a wrong regular PIN
    // adds only the cost of one SHA-256 pass before the normal PBKDF2
    // check, which is negligible compared with the regular PIN cost.
    // On a MATCH, though, that same speed would make this path finish
    // almost instantly next to every other outcome (~10s) unless
    // something evens it back out -- see consume_pbkdf2_time() below.
    if (pin::has_duress_pin() && pin::verify_duress(pin_guess)) {
        // verify_duress() is deliberately fast (SHA-256, not PBKDF2)
        // -- without this, a triggered duress PIN would complete
        // almost instantly compared to every other unlock attempt
        // (~10s), which is exactly the kind of observable difference
        // this feature exists to avoid. See
        // pin::consume_pbkdf2_time()'s own doc comment.
        pin::consume_pbkdf2_time();

        // The actual vault wipe happens one layer up
        // (ui::screens::LockScreen), which is allowed to depend on
        // vault:: -- security:: must not (see vault.hpp's own file
        // comment on the dependency direction). Transitioning to
        // Unlocked HERE, exactly like a real success, is the whole
        // point -- see pin_manager.hpp's DuressTriggered comment.
        transition_to_unlocked();
        ESP_LOGW(TAG, "Duress PIN entered -- proceeding as a normal unlock; caller must wipe the vault");
        return pin::VerifyResult::DuressTriggered;
    }

    const pin::VerifyResult result = pin::verify(pin_guess);

    if (result == pin::VerifyResult::Success) {
        transition_to_unlocked();
    }

    return result;
}

bool unlock_after_pin_set()
{
    if (!initialized || !pin::has_pin()) {
        return false;
    }

    transition_to_unlocked();
    return true;
}

void lock()
{
    if (!initialized || current_state == State::Locked) {
        return;
    }

    current_state = State::Locked;
    session::end_session();
    // The device being Locked must never leave a usable vault
    // encryption key sitting in memory -- see vault_key.hpp's own
    // file comment.
    vault_key::clear();

    if (event_bus::is_initialized()) {
        event_bus::publish(event_bus::Category::System,
                            static_cast<uint32_t>(event_bus::SystemEventId::DeviceLocked));
    }

    ESP_LOGI(TAG, "Locked");
    fire_callbacks(State::Locked);
}

void notify_activity()
{
    last_notified_activity_ms = now_ms();
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
