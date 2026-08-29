#include "input/input.hpp"

#include "input/button.hpp"
#include "input/encoder.hpp"

#include "bsp/pins.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace input {

namespace {

constexpr char TAG[] = "input";

constexpr uint32_t POLL_PERIOD_MS = 5;
constexpr uint32_t TASK_STACK_SIZE = 3072;
constexpr uint8_t TASK_PRIORITY = 5;
constexpr UBaseType_t EVENT_QUEUE_LEN = 16;

bool initialized = false;

Encoder encoder;
Button button_ok;
Button button_back;

QueueHandle_t event_queue = nullptr;
TaskHandle_t task_handle = nullptr;

// Updated on every event, independent of whether/when a consumer
// drains event_queue. See input::last_activity_ms().
volatile uint32_t activity_ms = 0;

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void push_event(EventType type, int32_t encoder_delta = 0)
{
    Event ev{type, encoder_delta, now_ms()};
    activity_ms = ev.timestamp_ms;

    // Never block the input task on a full queue: if a consumer has
    // fallen behind, drop the oldest event rather than stall input
    // handling for the whole firmware.
    if (xQueueSend(event_queue, &ev, 0) != pdPASS) {
        Event discarded{};
        xQueueReceive(event_queue, &discarded, 0);
        xQueueSend(event_queue, &ev, 0);
    }
}

/**
 * @brief Translate a generic Button event into a semantic input event
 *        for a specific physical button, and push it.
 */
void handle_button_event(ButtonEvent be,
                          EventType down,
                          EventType click,
                          EventType long_press,
                          EventType repeat,
                          EventType up)
{
    switch (be) {
        case ButtonEvent::None:
            return;
        case ButtonEvent::Down:
            push_event(down);
            return;
        case ButtonEvent::Click:
            push_event(click);
            return;
        case ButtonEvent::LongPress:
            push_event(long_press);
            return;
        case ButtonEvent::Repeat:
            push_event(repeat);
            return;
        case ButtonEvent::Up:
            push_event(up);
            return;
    }
}

void input_task(void* /*arg*/)
{
    const TickType_t period = pdMS_TO_TICKS(POLL_PERIOD_MS);

    while (true) {
        const int32_t delta = encoder.take_delta();
        if (delta != 0) {
            push_event(EventType::EncoderRotate, delta);
        }

        handle_button_event(
            button_ok.poll(),
            EventType::ButtonOkDown, EventType::ButtonOkClick,
            EventType::ButtonOkLongPress, EventType::ButtonOkRepeat,
            EventType::ButtonOkUp);

        handle_button_event(
            button_back.poll(),
            EventType::ButtonBackDown, EventType::ButtonBackClick,
            EventType::ButtonBackLongPress, EventType::ButtonBackRepeat,
            EventType::ButtonBackUp);

        vTaskDelay(period);
    }
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "input::init() called more than once, ignoring");
        return true;
    }

    Encoder::Config enc_cfg{};
    enc_cfg.pin_a = bsp::pins::ENCODER_A;
    enc_cfg.pin_b = bsp::pins::ENCODER_B;

    if (!encoder.init(enc_cfg)) {
        ESP_LOGE(TAG, "Encoder init failed");
        return false;
    }

    Button::Config ok_cfg{};
    ok_cfg.pin = bsp::pins::BUTTON_OK;
    if (!button_ok.init(ok_cfg)) {
        ESP_LOGE(TAG, "OK button init failed");
        return false;
    }

    Button::Config back_cfg{};
    back_cfg.pin = bsp::pins::BUTTON_BACK;
    if (!button_back.init(back_cfg)) {
        ESP_LOGE(TAG, "BACK button init failed");
        return false;
    }

    event_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(Event));
    if (event_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create input event queue");
        return false;
    }

    const BaseType_t task_created = xTaskCreate(
        input_task, "input", TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &task_handle);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create input task");
        vQueueDelete(event_queue);
        event_queue = nullptr;
        return false;
    }

    initialized = true;
    activity_ms = now_ms();
    ESP_LOGI(TAG, "Input initialized: encoder A=GPIO%d B=GPIO%d, OK=GPIO%d, BACK=GPIO%d",
             static_cast<int>(bsp::pins::ENCODER_A), static_cast<int>(bsp::pins::ENCODER_B),
             static_cast<int>(bsp::pins::BUTTON_OK), static_cast<int>(bsp::pins::BUTTON_BACK));

    return true;
}

bool is_initialized()
{
    return initialized;
}

bool poll_event(Event& out, uint32_t timeout_ms)
{
    if (!initialized) {
        return false;
    }

    const TickType_t ticks =
        (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    return xQueueReceive(event_queue, &out, ticks) == pdPASS;
}

void flush_events()
{
    if (initialized) {
        xQueueReset(event_queue);
    }
}

uint32_t last_activity_ms()
{
    return activity_ms;
}

} // namespace input
