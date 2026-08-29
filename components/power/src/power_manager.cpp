#include "power_manager.hpp"

#include "bsp/pins.hpp"
#include "input/input.hpp"

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
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(TASK_POLL_MS));

        const uint32_t input_activity = input::last_activity_ms();
        if (input_activity > last_activity_ms_) {
            last_activity_ms_ = input_activity;
        }

        if (cfg_.idle_timeout_ms > 0 && state_ == State::Active) {
            const uint32_t idle_for = now_ms() - last_activity_ms_;
            if (idle_for >= cfg_.idle_timeout_ms) {
                transition_to_light_sleep();
            }
        }
    }
}

void Manager::notify_activity()
{
    last_activity_ms_ = now_ms();
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
