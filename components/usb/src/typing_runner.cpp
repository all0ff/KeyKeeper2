#include "usb/typing_runner.hpp"

namespace usb {

namespace {

constexpr char ERR_NOT_CONNECTED[] = "USB not connected";
constexpr char ERR_UNSUPPORTED[] = "Unsupported character";

void wait(KeyIo& io, uint32_t ms)
{
    if (ms > 0) {
        io.delay_ms(ms);
    }
}

/// The layout-switch hotkey: wait, press, settle. Returns false if the press
/// itself failed (the settle wait is then skipped, as before).
bool send_hotkey(const HidEvent& ev, KeyIo& io, const char*& error)
{
    wait(io, ev.pre_ms);
    if (!io.send_key(ev.keycode, ev.modifier, ev.hold_ms)) {
        error = io.last_error();
        return false;
    }
    wait(io, ev.gap_ms);
    return true;
}

} // namespace

RunResult run_plan(const TypingPlan& plan, KeyIo& io)
{
    RunResult result;
    bool failed = false;
    bool run_open = false; // a Cyrillic run was opened and not yet closed

    for (const HidEvent& ev : plan.events) {
        if (failed) {
            // Everything after a failure is dropped -- except closing a run
            // that was already opened: leaving the host on the wrong layout
            // would turn the NEXT thing typed into wrong characters.
            if (ev.kind == HidEvent::Kind::LayoutOff && run_open) {
                send_hotkey(ev, io, result.error); // result ignored, as before
                run_open = false;
            }
            continue;
        }

        switch (ev.kind) {
            case HidEvent::Kind::Key:
                if (!io.ready()) {
                    result.error = ERR_NOT_CONNECTED;
                    failed = true;
                    break;
                }
                wait(io, ev.pre_ms);
                if (!io.send_key(ev.keycode, ev.modifier, ev.hold_ms)) {
                    result.error = io.last_error();
                    failed = true;
                    break;
                }
                wait(io, ev.gap_ms);
                result.chars_sent += ev.chars;
                break;

            case HidEvent::Kind::SkipLatin:
            case HidEvent::Kind::SkipCyrillic:
                // Readiness is checked first, exactly as the old type_char() /
                // type_cyrillic_char() did, so an unsupported character typed
                // while unplugged is still a "not connected" failure.
                if (!io.ready()) {
                    result.error = ERR_NOT_CONNECTED;
                    failed = true;
                    break;
                }
                if (ev.kind == HidEvent::Kind::SkipLatin) {
                    result.error = ERR_UNSUPPORTED;
                }
                result.chars_sent += ev.chars;
                break;

            case HidEvent::Kind::Pause:
                wait(io, ev.gap_ms);
                break;

            case HidEvent::Kind::LayoutOn:
                if (!send_hotkey(ev, io, result.error)) {
                    failed = true;
                } else {
                    run_open = true;
                }
                break;

            case HidEvent::Kind::LayoutOff:
                // Not failed: the hotkey is sent, a failure is ignored (but its
                // error text is recorded by send_hotkey()).
                send_hotkey(ev, io, result.error);
                run_open = false;
                break;
        }
    }

    result.ok = !failed;
    return result;
}

} // namespace usb
