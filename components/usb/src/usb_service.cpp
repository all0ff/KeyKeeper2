#include "usb/usb_service.hpp"
#include "usb/hid_keyboard.hpp"

#include "security/permission_manager.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>
#include <memory>
#include <string>

namespace usb {

namespace {

constexpr char TAG[] = "usb.svc";

constexpr uint32_t TYPE_TASK_STACK = 4096;
constexpr uint8_t  TYPE_TASK_PRIO  = 3;

TypeEngine engine;
const char* status_msg = "";

void set_status(const char* msg)
{
    status_msg = msg;
    ESP_LOGI(TAG, "%s", msg);
}

struct TypeTaskParams {
    std::string text;
    TypeEngine::Timing timing;
};

void type_task(void* arg)
{
    auto* params = static_cast<TypeTaskParams*>(arg);
    const size_t sent = engine.type_string(params->text, params->timing);

    if (sent == params->text.size()) {
        set_status("Typed OK");
    } else if (sent > 0) {
        set_status("Partially typed");
    } else {
        set_status(engine.last_error());
    }

    delete params;
    vTaskDelete(nullptr);
}

void spawn_type_task(const std::string& text, const TypeEngine::Timing& timing = TypeEngine::Timing{})
{
    auto* params = new TypeTaskParams{text, timing};
    const BaseType_t created = xTaskCreate(
        type_task, "usb_type", TYPE_TASK_STACK, params, TYPE_TASK_PRIO, nullptr);

    if (created != pdPASS) {
        set_status("Failed to start typing task");
        delete params;
    }
}

} // namespace

bool init()
{
    if (!hid::init()) {
        return false;
    }
    ESP_LOGI(TAG, "USB service initialised");
    return true;
}

bool is_connected()
{
    return hid::is_connected();
}

const char* last_status()
{
    return status_msg;
}

void type_string(const std::string& text)
{
    if (!is_connected()) {
        set_status("USB not connected");
        return;
    }
    spawn_type_task(text);
}

void print_field(const vault::VaultEntry& entry, Field field)
{
    if (!is_connected()) {
        set_status("USB not connected");
        return;
    }

    std::string text;
    switch (field) {
        case Field::Url:
            text = entry.url;
            break;
        case Field::Login:
            text = entry.login;
            break;
        case Field::Password:
            text = entry.password;
            break;
        case Field::Otp:
            // TODO: generate TOTP code from entry.totp_secret
            text = "123456"; // placeholder until TOTP is implemented
            break;
        case Field::LoginAndPassword:
            text = entry.login + "\t" + entry.password;
            break;
    }

    if (text.empty()) {
        set_status("Field is empty");
        return;
    }

    spawn_type_task(text);
}

} // namespace usb
