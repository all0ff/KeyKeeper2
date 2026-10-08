#include "usb/output_sink.hpp"

#include "usb/hid_keyboard.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace usb {

namespace {

/// KeyIo over the real USB HID endpoint: the same three calls the old
/// TypeEngine made directly (hid::is_connected, hid::send_key, vTaskDelay).
class HidKeyIo final : public KeyIo {
public:
    bool ready() override { return hid::is_connected(); }
    bool send_key(uint8_t keycode, uint8_t modifier, uint32_t hold_ms) override
    {
        return hid::send_key(keycode, modifier, hold_ms);
    }
    void delay_ms(uint32_t ms) override { vTaskDelay(pdMS_TO_TICKS(ms)); }
    const char* last_error() const override { return hid::last_error(); }
};

class UsbCableSink final : public OutputSink {
public:
    bool available() override { return io_.ready(); }
    RunResult play(const TypingPlan& plan) override { return run_plan(plan, io_); }

private:
    HidKeyIo io_;
};

} // namespace

OutputSink& usb_cable_sink()
{
    static UsbCableSink sink;
    return sink;
}

} // namespace usb
