#include "ui/ui.hpp"

#include "ui/screens/quick_screen.hpp"
#include "ui/screens/setup_pin_screen.hpp"
#include "ui/ui_manager.hpp"

#include "display/lvgl_port.hpp"
#include "input/input.hpp"
#include "security/pin_manager.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

#include <memory>

namespace ui {

namespace {

constexpr char TAG[] = "ui";

constexpr uint32_t TASK_STACK_SIZE = 4096;
constexpr uint8_t TASK_PRIORITY = 4;
constexpr uint32_t POLL_TIMEOUT_MS = 50;

bool initialized = false;
UiManager manager;
TaskHandle_t task_handle = nullptr;

void dispatch_event(const input::Event& event)
{
    switch (event.type) {
        case input::EventType::EncoderRotate: {
            const int32_t steps = event.encoder_delta;
            const InputAction action = steps > 0 ? InputAction::RotateRight : InputAction::RotateLeft;
            for (int32_t i = 0; i < (steps > 0 ? steps : -steps); ++i) {
                manager.handle_input(action);
            }
            break;
        }
        case input::EventType::ButtonOkClick:
            manager.handle_input(InputAction::OkShort);
            break;
        case input::EventType::ButtonOkLongPress:
            manager.handle_input(InputAction::OkLong);
            break;
        case input::EventType::ButtonBackClick:
            manager.handle_input(InputAction::BackShort);
            break;
        case input::EventType::ButtonBackLongPress:
            manager.handle_input(InputAction::BackLong);
            break;
        default:
            break;
    }
}

void ui_task(void* /*arg*/)
{
    while (true) {
        input::Event event;
        if (input::poll_event(event, POLL_TIMEOUT_MS)) {
            lvgl_port::lock();
            dispatch_event(event);
            lvgl_port::unlock();
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

    if (!lvgl_port::is_initialized()) {
        ESP_LOGE(TAG, "lvgl_port::init() must succeed before ui::init()");
        return false;
    }

    lvgl_port::lock();
    const bool manager_ok = manager.init(lv_screen_active());
    if (manager_ok) {
        // First boot: no PIN configured -> force setup
        // Otherwise: normal QuickScreen
        if (!security::pin::has_pin()) {
            ESP_LOGI(TAG, "First boot: no PIN set, showing SetupPinScreen");
            manager.push(std::make_unique<screens::SetupPinScreen>());
        } else {
            manager.push(std::make_unique<screens::QuickScreen>());
        }
    }
    lvgl_port::unlock();

    if (!manager_ok) {
        ESP_LOGE(TAG, "UiManager::init() failed");
        return false;
    }

    const BaseType_t task_created = xTaskCreate(
        ui_task, "ui", TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &task_handle);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ui task");
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "UI initialized");
    return true;
}

bool is_initialized()
{
    return initialized;
}

} // namespace ui
