#include "usb/type_engine.hpp"
#include "usb/hid_keyboard.hpp"
#include "usb/keycode_map.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace usb {

namespace {
constexpr char TAG[] = "usb.type";
}

bool TypeEngine::can_type() const
{
    return hid::is_connected();
}

const char* TypeEngine::last_error() const
{
    return last_error_;
}

bool TypeEngine::type_char(char c, const Timing& timing)
{
    if (!hid::is_connected()) {
        last_error_ = "USB not connected";
        return false;
    }

    const KeyMapping km = ascii_to_hid(c);
    if (km.keycode == keycode::NONE) {
        // Character not supported on US keyboard (e.g. Cyrillic).
        // Skip silently -- future Russian layout support will handle this.
        ESP_LOGW(TAG, "Unsupported character: 0x%02X, skipping", static_cast<unsigned char>(c));
        return true; // not a hard failure
    }

    const bool ok = hid::send_key(km.keycode, km.modifier, timing.press_ms);
    if (!ok) {
        last_error_ = hid::last_error();
        return false;
    }

    if (timing.inter_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(timing.inter_ms));
    }

    return true;
}

size_t TypeEngine::type_string(const std::string& text, const Timing& timing)
{
    if (!hid::is_connected()) {
        last_error_ = "USB not connected";
        ESP_LOGW(TAG, "type_string: USB not connected");
        return 0;
    }

    size_t sent = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        if (!type_char(text[i], timing)) {
            break;
        }
        ++sent;

        // Optional chunk delay for very long strings
        if (timing.chunk_ms > 0 && (i + 1) % 32 == 0) {
            vTaskDelay(pdMS_TO_TICKS(timing.chunk_ms));
        }
    }

    ESP_LOGI(TAG, "Typed %zu/%zu characters", sent, text.size());
    return sent;
}

} // namespace usb
