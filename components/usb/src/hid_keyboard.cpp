#include "usb/hid_keyboard.hpp"
#include "usb/keycode_map.hpp"

#include "tusb.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
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

// =============================================================================
// The actual root cause of a real, confirmed-on-hardware bug (device
// sent every HID report successfully, including Enter, yet the host
// only visibly registered a fraction of them): per TinyUSB's own
// documentation and reference example (examples/device/hid_composite),
// tud_hid_keyboard_report() returning true only means the report was
// ACCEPTED INTO TinyUSB's own internal buffer -- not that it was
// actually transferred over the wire and received by the host.
// TinyUSB's own fix for exactly this is
// tud_hid_report_complete_cb() -- "Invoked when sent REPORT
// successfully to host" -- which this file didn't implement before.
//
// report_complete_sem turns "accepted" into "actually confirmed
// delivered" for send_report() below to wait on. Binary, not
// counting: only ever one report in flight at a time by this code's
// own design, so there is never more than one completion to wait for.
//
// SIGNATURE RISK: the `len` parameter's type (uint8_t vs uint16_t)
// has genuinely differed between TinyUSB versions and broken this
// exact callback for other projects before (a mismatched override
// silently never gets called -- see espressif/arduino-esp32#7218).
// Written here as uint16_t, the current upstream convention.
// class/hid/hid_device.h is already #included above, so if this
// version's copy declares uint8_t instead, the compiler will fail to
// build this exact line with a conflicting-declaration error naming
// the expected type -- if that happens, change uint16_t to uint8_t
// right here and nothing else needs to change.
SemaphoreHandle_t report_complete_sem = nullptr;

} // namespace

extern "C" {

void tud_hid_report_complete_cb(uint8_t /*instance*/, uint8_t const* /*report*/, uint16_t /*len*/)
{
    if (report_complete_sem != nullptr) {
        xSemaphoreGive(report_complete_sem);
    }
}

} // extern "C"

namespace {

// How long to wait for the HID IN endpoint to become free before
// giving up on one report. The endpoint is polled by the host every
// 10 ms (bInterval in hid_configuration_descriptor above), so a
// healthy wait is a handful of ms; this only ever expires if the host
// has genuinely stopped polling.
constexpr uint32_t HID_READY_TIMEOUT_MS = 200;
// How long to wait for tud_hid_report_complete_cb() to actually fire
// after a successful tud_hid_keyboard_report() accept, before giving
// up on THIS attempt and retrying the whole send.
constexpr uint32_t REPORT_COMPLETE_TIMEOUT_MS = 200;
constexpr uint8_t SEND_ATTEMPTS = 6;
constexpr uint32_t RETRY_DELAY_MS = 5;
// Gap after a complete press/release pair. Particularly important for
// TAB and ENTER because they separate fields and the host may
// immediately move focus after consuming them.
constexpr uint32_t REPORT_GAP_MS = 25;

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

// The ONE place a keyboard report is actually queued -- and, now,
// actually confirmed delivered. Previously send_key()/release_all()
// called tud_hid_keyboard_report() directly and threw its return
// value away, with no confirmation step at all; see
// report_complete_sem's own comment above for why that was never
// actually proof the host received anything.
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

        // Drain any stale give() from a previous, unrelated completion
        // -- starting from a definitely-empty semaphore is what makes
        // the wait below trustworthy rather than possibly returning
        // instantly on a leftover signal.
        xSemaphoreTake(report_complete_sem, 0);

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

const uint8_t NO_KEYS[6] = { 0, 0, 0, 0, 0, 0 };

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

    // Created before tinyusb_driver_install() -- tud_hid_report_complete_cb()
    // could in principle fire as soon as the driver is up, and it must
    // never see report_complete_sem still null.
    report_complete_sem = xSemaphoreCreateBinary();
    if (report_complete_sem == nullptr) {
        set_error("Failed to create report-complete semaphore");
        return false;
    }

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
    // tud_hid_ready() deliberately NOT required here -- that reflects
    // whether the endpoint happens to be free for a report at this
    // exact instant, a transient per-report concern send_report()'s
    // own wait_hid_ready() already handles with retries. Requiring it
    // here too meant is_connected() could read false for a genuinely,
    // continuously connected device just because the endpoint was
    // momentarily busy -- confirmed on real hardware as a real source
    // of spurious "USB not connected" status, not just a theoretical
    // risk.
    return tud_mounted();
}

bool send_key(uint8_t keycode, uint8_t modifier, uint32_t press_ms)
{
    const uint8_t pressed[6] = { keycode, 0, 0, 0, 0, 0 };

    // A key is always a complete HID transaction: press -> release.
    // Both reports use the same retry+confirm path. The gap after
    // release is intentional: it prevents a field-control key from
    // being lost or merged with the next character on slower USB
    // hosts.
    if (!send_report(modifier, pressed)) {
        return false;
    }

    if (press_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(press_ms));
    }

    if (!send_report(0, NO_KEYS)) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(REPORT_GAP_MS));
    return true;
}

void release_all()
{
    send_report(0, NO_KEYS);
    vTaskDelay(pdMS_TO_TICKS(REPORT_GAP_MS));
}

const char* last_error()
{
    return error_string;
}

} // namespace usb::hid