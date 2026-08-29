#include "bsp/bsp.hpp"

#include "esp_log.h"

namespace bsp {

namespace {

constexpr char TAG[] = "bsp";

bool initialized = false;

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "bsp::init() called more than once, ignoring");
        return true;
    }

    const board::Info& info = board::info();

    ESP_LOGI(TAG, "Board: %s (rev %s)", info.identity.name, info.identity.revision);
    ESP_LOGI(TAG, "MCU: %s, Flash: %u MB, PSRAM: %u MB (octal: %s)",
             info.identity.mcu,
             static_cast<unsigned>(info.flash.size_mb),
             static_cast<unsigned>(info.psram.size_mb),
             info.psram.octal ? "yes" : "no");
    ESP_LOGI(TAG, "LCD: %ux%u, controller: %s",
             static_cast<unsigned>(info.lcd.width),
             static_cast<unsigned>(info.lcd.height),
             info.lcd.controller);

    /*
     * No peripheral GPIO is configured here by design. Every pin group
     * defined in bsp::pins is owned by exactly one higher-level
     * component (display, input, storage, system), which configures it
     * during its own init(). See bsp.hpp for the rationale.
     */

    initialized = true;
    return true;
}

bool is_initialized()
{
    return initialized;
}

const char* board_name()
{
    return board::info().identity.name;
}

const char* board_revision()
{
    return board::info().identity.revision;
}

} // namespace bsp
