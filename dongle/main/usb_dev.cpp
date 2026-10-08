#include "sdkconfig.h"

#if CONFIG_DONGLE_USB_HID

#include "usb_dev.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "class/hid/hid_device.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"

namespace dongle::usbdev {

namespace {

constexpr char TAG[] = "usbdev";

// ---- descriptors: CDC (interfaces 0, 1) + HID keyboard (interface 2)
enum { ITF_CDC = 0, ITF_CDC_DATA = 1, ITF_HID = 2, ITF_COUNT = 3 };
constexpr uint8_t EP_CDC_NOTIF = 0x81;
constexpr uint8_t EP_CDC_OUT = 0x02;
constexpr uint8_t EP_CDC_IN = 0x82;
constexpr uint8_t EP_HID_IN = 0x83;

#define DONGLE_USB_CONFIG_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

const uint8_t hid_report_descriptor[] = {TUD_HID_REPORT_DESC_KEYBOARD()};

const char* string_descriptor[] = {
    (char[]){0x09, 0x04}, // language: English
    "KeyKeeper2",         // 1 manufacturer
    "KeyKeeper2 Dongle",  // 2 product
    "000001",             // 3 serial
    "KeyKeeper2 Keyboard",// 4 HID
    "KeyKeeper2 Log",     // 5 CDC
};

const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, DONGLE_USB_CONFIG_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_CDC_DESCRIPTOR(ITF_CDC, 5, EP_CDC_NOTIF, 8, EP_CDC_OUT, EP_CDC_IN, 64),
    TUD_HID_DESCRIPTOR(ITF_HID, 4, false, sizeof(hid_report_descriptor), EP_HID_IN, 16, 10),
};

const char* error_string = "";

void set_error(const char* msg)
{
    error_string = msg;
    ESP_LOGW(TAG, "%s", msg);
}

// ---- HID: the same delivery rules as components/usb/src/hid_keyboard.cpp
SemaphoreHandle_t report_complete_sem = nullptr;

constexpr uint32_t HID_READY_TIMEOUT_MS = 200;
constexpr uint32_t REPORT_COMPLETE_TIMEOUT_MS = 200;
constexpr uint8_t SEND_ATTEMPTS = 6;
constexpr uint32_t RETRY_DELAY_MS = 5;
constexpr uint32_t REPORT_GAP_MS = 25;
constexpr uint32_t MODIFIER_STEP_MS = 20; // between the steps of a modifier combination

bool wait_hid_ready()
{
    const TickType_t start = xTaskGetTickCount();
    const TickType_t limit = pdMS_TO_TICKS(HID_READY_TIMEOUT_MS);
    while (!tud_hid_ready()) {
        if ((xTaskGetTickCount() - start) >= limit) {
            return false;
        }
        vTaskDelay(1);
    }
    return true;
}

bool send_report(uint8_t modifier, const uint8_t keycodes[6])
{
    if (!tud_mounted()) {
        set_error("USB not connected");
        return false;
    }
    for (uint8_t attempt = 0; attempt < SEND_ATTEMPTS; ++attempt) {
        if (!wait_hid_ready()) {
            if (attempt + 1 < SEND_ATTEMPTS) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
            set_error("HID endpoint stayed busy");
            return false;
        }
        xSemaphoreTake(report_complete_sem, 0); // forget a stale completion
        if (!tud_hid_keyboard_report(0, modifier, keycodes)) {
            if (attempt + 1 < SEND_ATTEMPTS) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            }
            continue;
        }
        if (xSemaphoreTake(report_complete_sem, pdMS_TO_TICKS(REPORT_COMPLETE_TIMEOUT_MS)) == pdTRUE) {
            return true;
        }
        if (attempt + 1 < SEND_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }
    }
    set_error("HID report never confirmed delivered");
    return false;
}

const uint8_t NO_KEYS[6] = {0, 0, 0, 0, 0, 0};

// ---- log -> serial port. A ring buffer keeps what was written while nobody listened.
constexpr size_t RING_SIZE = 8192;
uint8_t ring[RING_SIZE];
size_t ring_head = 0; // next write
size_t ring_len = 0;
portMUX_TYPE ring_lock = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t previous_vprintf = nullptr;

void ring_put(const char* s, size_t n)
{
    portENTER_CRITICAL(&ring_lock);
    for (size_t i = 0; i < n; ++i) {
        ring[ring_head] = static_cast<uint8_t>(s[i]);
        ring_head = (ring_head + 1) % RING_SIZE;
        if (ring_len < RING_SIZE) {
            ++ring_len;
        }
    }
    portEXIT_CRITICAL(&ring_lock);
}

// Copies up to `max` of the oldest bytes out and removes them. Returns the count.
size_t ring_take(uint8_t* out, size_t max)
{
    size_t n = 0;
    portENTER_CRITICAL(&ring_lock);
    const size_t tail = (ring_head + RING_SIZE - ring_len) % RING_SIZE;
    n = ring_len < max ? ring_len : max;
    for (size_t i = 0; i < n; ++i) {
        out[i] = ring[(tail + i) % RING_SIZE];
    }
    ring_len -= n;
    portEXIT_CRITICAL(&ring_lock);
    return n;
}

int log_vprintf(const char* fmt, va_list args)
{
    char buf[200];
    va_list copy;
    va_copy(copy, args);
    int n = std::vsnprintf(buf, sizeof buf, fmt, args);
    if (previous_vprintf != nullptr) {
        previous_vprintf(fmt, copy); // UART0 as well
    }
    va_end(copy);
    if (n > 0) {
        ring_put(buf, static_cast<size_t>(n) < sizeof buf ? static_cast<size_t>(n) : sizeof buf - 1);
    }
    return n;
}

} // namespace

extern "C" {

void tud_hid_report_complete_cb(uint8_t /*instance*/, uint8_t const* /*report*/, uint16_t /*len*/)
{
    if (report_complete_sem != nullptr) {
        xSemaphoreGive(report_complete_sem);
    }
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t /*instance*/)
{
    return hid_report_descriptor;
}

void tud_hid_set_report_cb(uint8_t /*itf*/, uint8_t /*report_id*/, hid_report_type_t /*report_type*/,
                           uint8_t const* /*buffer*/, uint16_t /*bufsize*/)
{
}

uint16_t tud_hid_get_report_cb(uint8_t /*itf*/, uint8_t /*report_id*/, hid_report_type_t /*report_type*/,
                               uint8_t* /*buffer*/, uint16_t /*reqlen*/)
{
    return 0;
}

} // extern "C"

bool init()
{
    report_complete_sem = xSemaphoreCreateBinary();
    if (report_complete_sem == nullptr) {
        set_error("Failed to create the report semaphore");
        return false;
    }
    // From here on the log is also kept for the serial port.
    previous_vprintf = esp_log_set_vprintf(log_vprintf);

    tinyusb_config_t config = TINYUSB_DEFAULT_CONFIG();
    config.descriptor.full_speed_config = configuration_descriptor;
    config.descriptor.string = string_descriptor;
    config.descriptor.string_count = sizeof(string_descriptor) / sizeof(string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    config.descriptor.high_speed_config = configuration_descriptor;
#endif
    const esp_err_t err = tinyusb_driver_install(&config);
    if (err != ESP_OK) {
        set_error("TinyUSB driver installation failed");
        ESP_LOGE(TAG, "tinyusb_driver_install() failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "USB keyboard + serial log ready");
    return true;
}

bool mounted()
{
    return tud_mounted();
}

bool send_key(uint8_t keycode, uint8_t modifier, uint32_t hold_ms)
{
    // A bare modifier combination (the layout hotkey Alt+Shift) is pressed and released the way a person
    // does it on a real keyboard: one modifier after another, released in reverse order. All modifiers in a
    // single report make the host see them in an arbitrary order, and Windows then ignores the hotkey
    // now and then.
    if (keycode == 0 && (modifier & (modifier - 1)) != 0) {
        static const uint8_t ORDER[] = {0x01, 0x10, 0x04, 0x40, 0x02, 0x20, 0x08, 0x80}; // Ctrl, Alt, Shift, Gui
        uint8_t steps[8];
        uint8_t levels[8];
        size_t n = 0;
        uint8_t acc = 0;
        for (uint8_t bit : ORDER) {
            if ((modifier & bit) != 0) {
                acc = static_cast<uint8_t>(acc | bit);
                steps[n] = bit;
                levels[n] = acc;
                ++n;
            }
        }
        for (size_t i = 0; i < n; ++i) {
            if (!send_report(levels[i], NO_KEYS)) {
                send_report(0, NO_KEYS);
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(MODIFIER_STEP_MS));
        }
        if (hold_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(hold_ms));
        }
        for (size_t i = n; i-- > 0;) {
            const uint8_t level = static_cast<uint8_t>(levels[i] & ~steps[i]);
            if (!send_report(level, NO_KEYS)) {
                send_report(0, NO_KEYS);
                return false;
            }
            if (i > 0) {
                vTaskDelay(pdMS_TO_TICKS(MODIFIER_STEP_MS));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(REPORT_GAP_MS));
        return true;
    }

    const uint8_t pressed[6] = {keycode, 0, 0, 0, 0, 0};
    if (!send_report(modifier, pressed)) {
        return false;
    }
    if (hold_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(hold_ms));
    }
    if (!send_report(0, NO_KEYS)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(REPORT_GAP_MS));
    return true;
}

const char* last_error()
{
    return error_string;
}

void pump()
{
    if (!tud_cdc_connected()) {
        return;
    }
    uint8_t chunk[64];
    bool wrote = false;
    for (int guard = 0; guard < 8; ++guard) {
        const uint32_t room = tud_cdc_write_available();
        if (room == 0) {
            break;
        }
        const size_t want = room < sizeof chunk ? room : sizeof chunk;
        const size_t n = ring_take(chunk, want);
        if (n == 0) {
            break;
        }
        tud_cdc_write(chunk, n);
        wrote = true;
    }
    if (wrote) {
        tud_cdc_write_flush();
    }
}

} // namespace dongle::usbdev

#endif // CONFIG_DONGLE_USB_HID
