#include "event_bus/event_bus.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace event_bus {

namespace {

constexpr char TAG[] = "event_bus";

constexpr UBaseType_t QUEUE_LEN = 32;
constexpr uint32_t TASK_STACK_SIZE = 3072;
constexpr uint8_t TASK_PRIORITY = 4;
constexpr size_t MAX_SUBSCRIBERS = 16;

bool initialized = false;

QueueHandle_t bus_queue = nullptr;
TaskHandle_t dispatcher_task_handle = nullptr;

struct Subscriber
{
    Category category;
    Handler handler = nullptr;
    void* ctx = nullptr;
    bool used = false;
};

Subscriber subscribers[MAX_SUBSCRIBERS]{};

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void dispatch(const Event& event)
{
    for (const Subscriber& sub : subscribers) {
        if (sub.used && sub.category == event.category && sub.handler != nullptr) {
            sub.handler(event, sub.ctx);
        }
    }
}

void dispatcher_task(void* /*arg*/)
{
    Event event{};
    while (true) {
        if (xQueueReceive(bus_queue, &event, portMAX_DELAY) == pdPASS) {
            dispatch(event);
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

    bus_queue = xQueueCreate(QUEUE_LEN, sizeof(Event));
    if (bus_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return false;
    }

    const BaseType_t task_created = xTaskCreate(
        dispatcher_task, "event_bus", TASK_STACK_SIZE, nullptr, TASK_PRIORITY,
        &dispatcher_task_handle);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create dispatcher task");
        vQueueDelete(bus_queue);
        bus_queue = nullptr;
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "Event bus initialized (queue depth %u, %u subscriber slots)",
             static_cast<unsigned>(QUEUE_LEN), static_cast<unsigned>(MAX_SUBSCRIBERS));

    return true;
}

bool is_initialized()
{
    return initialized;
}

bool publish(Event event)
{
    if (!initialized) {
        return false;
    }

    event.timestamp_ms = now_ms();

    if (xQueueSend(bus_queue, &event, 0) == pdPASS) {
        return true;
    }

    // Queue full -- a subscriber has fallen behind. Drop the oldest
    // event to make room rather than block the publisher, same policy
    // components/input uses for its own queue.
    Event discarded{};
    xQueueReceive(bus_queue, &discarded, 0);
    return xQueueSend(bus_queue, &event, 0) == pdPASS;
}

bool publish(Category category, uint32_t id, Payload payload)
{
    Event event{};
    event.category = category;
    event.id = id;
    event.payload = payload;
    return publish(event);
}

int subscribe(Category category, Handler handler, void* ctx)
{
    if (handler == nullptr) {
        return -1;
    }

    for (size_t i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (!subscribers[i].used) {
            subscribers[i] = {category, handler, ctx, true};
            return static_cast<int>(i);
        }
    }

    ESP_LOGW(TAG, "Subscriber table full (max %u)", static_cast<unsigned>(MAX_SUBSCRIBERS));
    return -1;
}

void unsubscribe(int handle)
{
    if (handle < 0 || static_cast<size_t>(handle) >= MAX_SUBSCRIBERS) {
        return;
    }
    subscribers[handle].used = false;
}

} // namespace event_bus
