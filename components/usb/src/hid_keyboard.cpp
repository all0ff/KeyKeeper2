#include "usb/hid_keyboard.hpp"
#include "usb/keycode_map.hpp"

#include "tusb.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "class/hid/hid_device.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace usb::hid {

namespace {
constexpr char TAG[] = "usb.hid";

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

static const char* hid_string_descriptor[] = {
    (char[]){0x09, 0x04},
    "KeyKeeper2",
    "KeyKeeper2 HID Keyboard",
    "KeyKeeper2",
    "Keyboard",
};

static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(
        1,
        1,
        0,
        TUSB_DESC_TOTAL_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        100
    ),

    TUD_HID_DESCRIPTOR(
        0,
        4,
        false,
        sizeof(hid_report_descriptor),
        0x81,
        16,
        10
    ),
};

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

uint8_t const* tud_hid_descriptor_report_cb(uint8_t /*instance*/)
{
    return hid_report_descriptor;
}

    void tud_hid_set_report_cb(uint8_t /*itf*/, uint8_t /*report_id*/,
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
    tinyusb_config_t config = TINYUSB_DEFAULT_CONFIG();

    config.descriptor.full_speed_config = hid_configuration_descriptor;
    config.descriptor.string = hid_string_descriptor;
    config.descriptor.string_count =
        sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);

#if (TUD_OPT_HIGH_SPEED)
    config.descriptor.high_speed_config = hid_configuration_descriptor;
#endif

    const esp_err_t err = tinyusb_driver_install(&config);

    if (err != ESP_OK) {
        set_error("TinyUSB driver installation failed");
        ESP_LOGE(TAG, "tinyusb_driver_install() failed: %s", esp_err_to_name(err));
        return false;
    }

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
    tud_hid_keyboard_report(0, modifier, report);

    if (press_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(press_ms));
    }

    release_all();
    return true;
}

void release_all()
{
    tud_hid_keyboard_report(0, 0, nullptr);
    vTaskDelay(pdMS_TO_TICKS(10));
}

const char* last_error()
{
    return error_string;
}

} // namespace usb::hid