#include "fake_hid.hpp"

#include "freertos/task.h"
#include "settings/settings.hpp"
#include "usb/hid_keyboard.hpp"

FakeHid g_hid;
static settings::Settings g_settings;
settings::Settings& settings::all() { return g_settings; }

void vTaskDelay(TickType_t ticks) { g_hid.trace += "D" + std::to_string(ticks) + ";"; }

namespace usb::hid {
bool init() { return true; }
bool is_connected()
{
    if (!g_hid.connected_at_start) return false;
    return g_hid.connected_for_sends < 0 || g_hid.sends < g_hid.connected_for_sends;
}
bool send_key(uint8_t keycode, uint8_t modifier, uint32_t press_ms)
{
    g_hid.trace += "K" + std::to_string(keycode) + "/" + std::to_string(modifier) + "/" + std::to_string(press_ms) + ";";
    const bool ok = (g_hid.fail_send_at < 0) || (g_hid.sends != g_hid.fail_send_at);
    ++g_hid.sends;
    return ok;
}
void release_all() {}
const char* last_error() { return "fake send failure"; }
} // namespace usb::hid
