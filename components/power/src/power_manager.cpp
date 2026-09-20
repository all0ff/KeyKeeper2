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
constexpr uint32_t DIAGNOSTIC_LOG_PERIOD_MS = 5000;

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
    uint32_t last_logged_timeout_s = UINT32_MAX;
    bool last_logged_backlight_off = false;
    uint32_t last_diagnostic_log_ms = now_ms();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(TASK_POLL_MS));

        const uint32_t input_activity = input::last_activity_ms();
        const bool activity_advanced = input_activity > last_activity_ms_;
        if (activity_advanced) {
            ESP_LOGI(TAG, "Input activity advanced: %u -> %u ms",
                     static_cast<unsigned>(last_activity_ms_),
                     static_cast<unsigned>(input_activity));
            last_activity_ms_ = input_activity;
        }

        // Screen-off-on-idle -- see power.hpp's file comment for why
        // this is a plain backlight action (no power::State change,
        // no esp_sleep involvement) rather than the automatic light
        // sleep this used to trigger. Read live every poll, not
        // captured once at init(), so a settings change takes effect
        // immediately without needing to reinitialize this component.
        const uint32_t display_off_timeout_s = settings::all().general.display_off_timeout_s;

        if (display_off_timeout_s != last_logged_timeout_s) {
            ESP_LOGI(TAG, "Screen timeout setting: %u s",
                     static_cast<unsigned>(display_off_timeout_s));
            last_logged_timeout_s = display_off_timeout_s;
        }

        if (activity_advanced && backlight_off_) {
            ESP_LOGI(TAG, "Wake from input activity: turning backlight ON");
            display::set_backlight(true);
            backlight_off_ = false;
        } else if (!backlight_off_ && display_off_timeout_s > 0) {
            const uint32_t idle_for = now_ms() - last_activity_ms_;
            if (idle_for >= display_off_timeout_s * 1000u) {
                ESP_LOGI(TAG, "Screen timeout reached: idle=%u ms, turning backlight OFF",
                         static_cast<unsigned>(idle_for));
                display::set_backlight(false);
                backlight_off_ = true;
            }
        }

        const uint32_t now = now_ms();
        if (now - last_diagnostic_log_ms >= DIAGNOSTIC_LOG_PERIOD_MS) {
            const uint32_t idle_for = now - last_activity_ms_;
            ESP_LOGI(TAG,
                     "Screen diagnostic: timeout=%u s, idle=%u ms, backlight_off=%s, activity=%u ms",
                     static_cast<unsigned>(display_off_timeout_s),
                     static_cast<unsigned>(idle_for),
                     backlight_off_ ? "true" : "false",
                     static_cast<unsigned>(last_activity_ms_));
            last_diagnostic_log_ms = now;
        }

        if (backlight_off_ != last_logged_backlight_off) {
            ESP_LOGI(TAG, "Logical backlight state changed: off=%s",
                     backlight_off_ ? "true" : "false");
            last_logged_backlight_off = backlight_off_;
        }
    }
}

void Manager::notify_activity()
{
    ESP_LOGI(TAG, "notify_activity() called");
    last_activity_ms_ = now_ms();

    if (backlight_off_) {
        ESP_LOGI(TAG, "notify_activity(): turning backlight ON");
        display::set_backlight(true);
        backlight_off_ = false;
    }
}

int Manager::register_callback(Callback cb, void* ctx)
{
    if (cb == nullptr) {
        return -1;
    }

    for (size_t i = 0; i < MAX_CALLBACKS; ++i) {
        if (!callbacks_[i].used) {
            callbacks_[i] = {cb, ctx, true};
            return static_cast<int>(i);
        }
    }

    ESP_LOGW(TAG, "Callback registry full (max %u)", static_cast<unsigned>(MAX_CALLBACKS));
    return -1;
}

void Manager::unregister_callback(int handle)
{
    if (handle < 0 || static_cast<size_t>(handle) >= MAX_CALLBACKS) {
        return;
    }
    callbacks_[handle].used = false;
}

void Manager::fire_callbacks(State new_state)
{
    for (const CallbackSlot& slot : callbacks_) {
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
    xSemaphoreTake(transition_mutex_, portMAX_DELAY);

    // Another task may have already handled this (e.g. request_sleep()
    // raced with the idle-timeout check). Nothing to do if we're not
    // Active anymore by the time we get the mutex.
    if (state_ != State::Active) {
        xSemaphoreGive(transition_mutex_);
        return;
    }

    ESP_LOGI(TAG, "Entering light sleep");
    fire_callbacks(State::LightSleep);
    state_ = State::LightSleep;

    configure_wake_sources_light_sleep();
    esp_light_sleep_start(); // blocks here until a wake source fires

    state_ = State::Active;
    last_activity_ms_ = now_ms();
    ESP_LOGI(TAG, "Woke from light sleep");
    fire_callbacks(State::Active);

    xSemaphoreGive(transition_mutex_);
}

void Manager::transition_to_deep_sleep()
{
    xSemaphoreTake(transition_mutex_, portMAX_DELAY);

    ESP_LOGI(TAG, "Entering deep sleep (shutdown)");
    fire_callbacks(State::DeepSleep);
    state_ = State::DeepSleep;

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
