#include "power_manager.hpp"

#include "bsp/pins.hpp"
#include "display/display.hpp"
#include "input/input.hpp"
#include "settings/settings.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"

namespace power::internal {

namespace {

constexpr char TAG[] = "power";

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

} // namespace

Manager& instance()
{
    static Manager manager;
    return manager;
}

bool Manager::init(const Config& cfg)
{
    if (task_handle_ != nullptr) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!input::is_initialized()) {
        ESP_LOGE(TAG, "input::init() must succeed before power::init()");
        return false;
    }

    cfg_ = cfg;
    state_ = State::Active;
    last_activity_ms_ = now_ms();

    transition_mutex_ = xSemaphoreCreateMutex();
    if (transition_mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create transition mutex");
        return false;
    }

    const BaseType_t task_created = xTaskCreate(
        task_trampoline, "power", TASK_STACK_SIZE, this, TASK_PRIORITY, &task_handle_);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create power task");
        vSemaphoreDelete(transition_mutex_);
        transition_mutex_ = nullptr;
        task_handle_ = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Power manager initialized (idle_timeout=%u ms)",
             static_cast<unsigned>(cfg_.idle_timeout_ms));

    return true;
}

void Manager::task_trampoline(void* arg)
{
    static_cast<Manager*>(arg)->task();
}

void Manager::task()
{
    uint32_t poll_count = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(TASK_POLL_MS));
        ++poll_count;

        const uint32_t input_activity = input::last_activity_ms();
        const bool activity_advanced = input_activity > last_activity_ms_;
        if (activity_advanced) {
            last_activity_ms_ = input_activity;
        }

        // Screen-off-on-idle -- see power.hpp's file comment for why
        // this is a plain backlight action (no power::State change,
        // no esp_sleep involvement) rather than the automatic light
        // sleep this used to trigger. Read live every poll, not
        // captured once at init(), so a settings change takes effect
        // immediately without needing to reinitialize this component.
        const uint32_t display_off_timeout_s = settings::all().general.display_off_timeout_s;
        const uint32_t idle_for = now_ms() - last_activity_ms_;

        // DIAGNOSTIC -- every ~2s (10 polls at TASK_POLL_MS=200) while
        // idle-tracking is even possible (timeout > 0 and not already
        // asleep), so a captured log directly shows whether this loop
        // is running at all and what it sees, rather than needing to
        // infer that from silence. Safe to leave in -- one line every
        // 2s is negligible, and it directly disproves or confirms
        // "the timeout check never runs" the next time this needs
        // diagnosing.
        if (display_off_timeout_s > 0 && !display::is_asleep() && (poll_count % 10) == 0) {
            ESP_LOGI(TAG, "idle_for=%lums threshold=%lums asleep=%d",
                     static_cast<unsigned long>(idle_for),
                     static_cast<unsigned long>(display_off_timeout_s) * 1000UL,
                     static_cast<int>(display::is_asleep()));
        }

        // display::is_asleep() (NOT is_backlight_enabled()) is asked
        // here -- a real, confirmed bug on actual hardware came from
        // using the brightness-derived is_backlight_enabled() for
        // this: setting Brightness to exactly 0 via General Settings
        // (legitimate, nothing to do with idle timeout) also made
        // THAT function return false, which made this exact branch
        // think the screen was already "asleep" from the very first
        // poll after that, so idle-timeout's own check (the other
        // branch below) never even ran again -- see
        // display::is_backlight_enabled()'s own comment for the full
        // chain, including how it ALSO permanently broke input
        // (every action treated as a wake-only gesture, never
        // reaching the UI). is_asleep() is a separate,
        // brightness-independent flag that only this Manager and
        // input:: 's own wake-detection touch, so a brightness of 0
        // can no longer be mistaken for idle-sleep.
        if (activity_advanced && display::is_asleep()) {
            ESP_LOGI(TAG, "Activity while asleep -- waking display");
            display::set_asleep(false);
        } else if (!display::is_asleep() && display_off_timeout_s > 0) {
            if (idle_for >= display_off_timeout_s * 1000u) {
                ESP_LOGI(TAG, "Idle for %lums >= %lums threshold -- putting display to sleep",
                         static_cast<unsigned long>(idle_for),
                         static_cast<unsigned long>(display_off_timeout_s) * 1000UL);
                display::set_asleep(true);
            }
        }
    }
}

void Manager::notify_activity()
{
    last_activity_ms_ = now_ms();

    if (display::is_asleep()) {
        display::set_asleep(false);
    }
}

int Manager::register_callback(Callback cb, void* ctx)
{
    if (cb == nullptr) {
        return -1;
    }

    // Protects callbacks_[] against fire_callbacks() (possibly a
    // different task, during a sleep/wake transition) reading it
    // mid-write -- not an audit-numbered finding on its own, found
    // alongside Q-05/Q-06 and closed the same way, reusing
    // transition_mutex_ rather than adding a second lock for what's
    // really the same "Manager's own internal state" this mutex
    // already protects.
    xSemaphoreTake(transition_mutex_, portMAX_DELAY);
    int result = -1;
    for (size_t i = 0; i < MAX_CALLBACKS; ++i) {
        if (!callbacks_[i].used) {
            callbacks_[i] = {cb, ctx, true};
            result = static_cast<int>(i);
            break;
        }
    }
    xSemaphoreGive(transition_mutex_);

    if (result < 0) {
        ESP_LOGW(TAG, "Callback registry full (max %u)", static_cast<unsigned>(MAX_CALLBACKS));
    }
    return result;
}

void Manager::unregister_callback(int handle)
{
    if (handle < 0 || static_cast<size_t>(handle) >= MAX_CALLBACKS) {
        return;
    }
    xSemaphoreTake(transition_mutex_, portMAX_DELAY);
    callbacks_[handle].used = false;
    xSemaphoreGive(transition_mutex_);
}

State Manager::state() const
{
    xSemaphoreTake(transition_mutex_, portMAX_DELAY);
    const State snapshot = state_;
    xSemaphoreGive(transition_mutex_);
    return snapshot;
}

void Manager::fire_callbacks(State new_state)
{
    // Snapshot the active slots under the lock, then invoke them
    // OUTSIDE it -- same reentrancy reasoning as
    // transition_to_light_sleep()'s own comment above: a callback is
    // free to call register_callback()/unregister_callback() (or
    // request_sleep(), re-entering this whole call chain), and doing
    // that while THIS function still held transition_mutex_ for the
    // actual cb(...) invocation would risk the identical deadlock
    // Q-06 already fixed one call site of, just moved here instead of
    // eliminated.
    CallbackSlot snapshot[MAX_CALLBACKS];
    xSemaphoreTake(transition_mutex_, portMAX_DELAY);
    for (size_t i = 0; i < MAX_CALLBACKS; ++i) {
        snapshot[i] = callbacks_[i];
    }
    xSemaphoreGive(transition_mutex_);

    for (const CallbackSlot& slot : snapshot) {
        if (slot.used && slot.cb != nullptr) {
            slot.cb(new_state, slot.ctx);
        }
    }
}

void Manager::request_sleep()
{
    transition_to_light_sleep();
}

void Manager::request_shutdown()
{
    transition_to_deep_sleep();
}

void Manager::transition_to_light_sleep()
{
    // fire_callbacks() below is now OUTSIDE every critical section in
    // this function -- audit finding Q-06, a real, confirmed-correct
    // deadlock risk: transition_mutex_ (xSemaphoreCreateMutex()) is
    // NOT recursive, so a callback that calls back into this Manager's
    // own public API (request_sleep() in particular, which calls
    // straight back into this same function) while still inside the
    // critical section that invoked it would block forever trying to
    // re-take a mutex this exact task already holds. state_ is set to
    // LightSleep and the mutex released BEFORE firing callbacks, in
    // that order specifically, so a reentrant call sees state_ !=
    // Active and takes the early-return path below instead of
    // deadlocking -- the callback-visible state is correct at the
    // moment callbacks observe it either way, this only changes
    // whether the mutex is held while they run.
    xSemaphoreTake(transition_mutex_, portMAX_DELAY);
    // Another task may have already handled this (e.g. request_sleep()
    // raced with the idle-timeout check). Nothing to do if we're not
    // Active anymore by the time we get the mutex.
    if (state_ != State::Active) {
        xSemaphoreGive(transition_mutex_);
        return;
    }
    state_ = State::LightSleep;
    xSemaphoreGive(transition_mutex_);

    ESP_LOGI(TAG, "Entering light sleep");
    fire_callbacks(State::LightSleep);

    configure_wake_sources_light_sleep();
    esp_light_sleep_start(); // blocks here until a wake source fires

    xSemaphoreTake(transition_mutex_, portMAX_DELAY);
    state_ = State::Active;
    last_activity_ms_ = now_ms();
    xSemaphoreGive(transition_mutex_);

    ESP_LOGI(TAG, "Woke from light sleep");
    fire_callbacks(State::Active);
}

void Manager::transition_to_deep_sleep()
{
    // Same reentrancy reasoning as transition_to_light_sleep() above
    // -- fire_callbacks() here runs after the mutex is released, not
    // while still holding it, for the same deadlock-avoidance reason.
    // No "restore state_ on return" step needed here specifically:
    // esp_deep_sleep_start() never returns, the chip resets and
    // re-runs app_main() instead.
    xSemaphoreTake(transition_mutex_, portMAX_DELAY);
    state_ = State::DeepSleep;
    xSemaphoreGive(transition_mutex_);

    ESP_LOGI(TAG, "Entering deep sleep (shutdown)");
    fire_callbacks(State::DeepSleep);

    configure_wake_sources_deep_sleep();

    // Never returns: the chip resets on wake and re-runs app_main().
    esp_deep_sleep_start();
}

void Manager::configure_wake_sources_light_sleep()
{
    // Any GPIO can wake from light sleep (unlike deep sleep, which
    // needs the RTC/EXT1 path). Wake on OK, BACK, or either encoder
    // phase going low (all wired active-low with pull-ups), so both a
    // button press and a knob turn bring the device back.
    gpio_wakeup_enable(bsp::pins::BUTTON_OK, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(bsp::pins::BUTTON_BACK, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(bsp::pins::ENCODER_A, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(bsp::pins::ENCODER_B, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
}

void Manager::configure_wake_sources_deep_sleep()
{
    // Deep sleep only wakes via the RTC/EXT1 path, which needs an
    // explicit pin bitmask. Deliberately limited to OK/BACK -- an
    // idle encoder twitch should not power the device back on from a
    // full shutdown, only a deliberate button press should.
    const uint64_t wake_mask =
        (1ULL << bsp::pins::BUTTON_OK) | (1ULL << bsp::pins::BUTTON_BACK);

    esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);
}

} // namespace power::internal
