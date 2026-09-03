#include "usb/hid_keyboard.hpp"

#include "usb/keycode_map.hpp"

#include "tusb.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace usb::hid {

namespace {
constexpr char TAG[] = "usb.hid";

const char* error_string = "";

void set_error(const char* msg)
{
    error_string = msg;
    ESP_LOGW(TAG, "%s", msg);
}

} // namespace

// ---------------------------------------------------------------------------
// TinyUSB callbacks (required by the stack, even if empty)
// ---------------------------------------------------------------------------
extern "C" {

void tud_hid_set_report_cb(uint8_t /*itf*/, uint8_t /*report_id",
                           hid_report_type_t /*report_type*/,
                           uint8_t const* /*buffer*/, uint16_t /*bufsize*/)
{
    // Not used for a keyboard-only device.
}

uint16_t tud_hid_get_report_cb(uint8_t /*itf*/, uint8_t /*report_id*/,
                               hid_report_type_t /*report_type*/,
                               uint8_t* /*buffer*/, uint16_t /*reqlen*/)
{
    return 0;
}

} // extern "C"

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool init()
{
    // TinyUSB is initialised by ESP-IDF automatically when
    // CONFIG_TINYUSB_HID_ENABLED is set in sdkconfig.  We only need
    // to verify that the HID interface is ready.
    // TinyUSB is initialised by ESP-IDF when CONFIG_TINYUSB is enabled.
    // No explicit init needed here.

    ESP_LOGI(TAG, "HID Keyboard ready");
    return true;
}

bool is_connected()
{
    return tud_mounted() && tud_hid_ready();
}

bool send_key(uint8_t keycode, uint8_t modifier, uint32_t press_ms)
{
    if (!is_connected()) {
        set_error("USB not connected");
        return false;
    }

    uint8_t report[6] = { keycode, 0, 0, 0, 0, 0 };
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifier, report);

    if (press_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(press_ms));
    }

    release_all();
    return true;
}

void release_all()
{
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, nullptr);
    vTaskDelay(pdMS_TO_TICKS(10));
}

const char* last_error()
{
    return error_string;
}

} // namespace usb::hid
