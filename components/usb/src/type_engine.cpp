#include "usb/type_engine.hpp"

#include "settings/settings.hpp"

#include "esp_log.h"

namespace usb {

namespace {
constexpr char TAG[] = "usb.type";
constexpr char ERR_NOT_CONNECTED[] = "USB not connected";

size_t count_skipped(const TypingPlan& plan)
{
    size_t n = 0;
    for (const HidEvent& ev : plan.events) {
        if (ev.kind == HidEvent::Kind::SkipLatin || ev.kind == HidEvent::Kind::SkipCyrillic) {
            ++n;
        }
    }
    return n;
}
} // namespace

bool TypeEngine::can_type() const
{
    return sink().available();
}

const char* TypeEngine::last_error() const
{
    return last_error_;
}

bool TypeEngine::type_char(char c, const Timing& timing)
{
    OutputSink& out = sink();
    if (!out.available()) {
        last_error_ = ERR_NOT_CONNECTED;
        return false;
    }

    PlanOptions options;
    options.timing = timing;
    const RunResult result = out.play(plan_events(std::string(1, c), options));
    if (result.error != nullptr) {
        last_error_ = result.error;
    }
    // An unsupported character is skipped, not a hard failure (as before).
    return result.ok;
}

size_t TypeEngine::type_string(const std::string& text, const Timing& timing)
{
    OutputSink& out = sink();
    if (!out.available()) {
        last_error_ = ERR_NOT_CONNECTED;
        ESP_LOGW(TAG, "type_string: USB not connected");
        return 0;
    }

    // See settings::UsbSettings::cyrillic_auto_switch_layout's own comment --
    // defaults to false (manual layout switching by the person, on the host,
    // themselves) given a real test found the switch-BACK after a run didn't
    // reliably register. When false, this device never touches Alt+Shift.
    PlanOptions options;
    options.timing = timing;
    options.auto_switch_layout = settings::all().usb.cyrillic_auto_switch_layout;

    const TypingPlan plan = plan_events(text, options);
    const size_t skipped = count_skipped(plan);
    if (skipped > 0) {
        ESP_LOGW(TAG, "%zu unsupported character(s) skipped", skipped);
    }

    const RunResult result = out.play(plan);
    if (result.error != nullptr) {
        last_error_ = result.error;
    }

    ESP_LOGI(TAG, "Typed %zu characters", result.chars_sent);
    return result.chars_sent;
}

} // namespace usb
