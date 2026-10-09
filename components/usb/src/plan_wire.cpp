#include "usb/plan_wire.hpp"

namespace usb {

namespace {

using kk::msg::Event;
using kk::msg::EventKind;

void push(WirePlan* w, const Event& e, size_t cum)
{
    w->events.push_back(e);
    w->chars_after.push_back(cum);
}

/// A wait of `ms`, split into Pause events of at most 255 ms.
void push_wait(WirePlan* w, uint32_t ms, size_t cum)
{
    while (ms > 0) {
        Event p;
        p.kind = EventKind::Pause;
        p.gap_ms = static_cast<uint8_t>(ms > 255 ? 255 : ms);
        ms -= p.gap_ms;
        push(w, p, cum);
    }
}

} // namespace

bool plan_to_wire(const TypingPlan& plan, WirePlan* out)
{
    if (out == nullptr) {
        return false;
    }
    *out = WirePlan();
    size_t cum = 0;
    for (const HidEvent& h : plan.events) {
        switch (h.kind) {
            case HidEvent::Kind::Key:
            case HidEvent::Kind::LayoutOn:
            case HidEvent::Kind::LayoutOff: {
                // The wait before the press stays INSIDE the event when it fits (a closing hotkey sent after a
                // failure keeps its wait); only the part above 255 ms becomes separate Pause events.
                const uint32_t pre_in_event = h.pre_ms > 255 ? 255 : h.pre_ms;
                push_wait(out, h.pre_ms - pre_in_event, cum);
                Event e;
                e.pre_ms = static_cast<uint8_t>(pre_in_event);
                e.kind = h.kind == HidEvent::Kind::Key ? EventKind::Key
                         : h.kind == HidEvent::Kind::LayoutOn ? EventKind::LayoutOn : EventKind::LayoutOff;
                e.mods = h.modifier;
                e.usage = h.keycode;
                uint32_t hold = h.hold_ms;
                if (hold == 0) {
                    hold = 1;
                    out->clamped = true;
                } else if (hold > 255) {
                    hold = 255;
                    out->clamped = true;
                }
                e.hold_ms = static_cast<uint8_t>(hold);
                const uint32_t gap_in_event = h.gap_ms > 255 ? 255 : h.gap_ms;
                e.gap_ms = static_cast<uint8_t>(gap_in_event);
                if (h.kind == HidEvent::Kind::Key) {
                    cum += h.chars;
                }
                if (!kk::msg::event_valid(e)) {
                    return false;
                }
                push(out, e, cum);
                push_wait(out, h.gap_ms - gap_in_event, cum);
                break;
            }
            case HidEvent::Kind::Pause:
                push_wait(out, h.gap_ms, cum);
                break;
            case HidEvent::Kind::SkipLatin:
            case HidEvent::Kind::SkipCyrillic:
                cum += h.chars;
                if (h.kind == HidEvent::Kind::SkipLatin) {
                    out->unsupported = true;
                }
                if (out->events.empty()) {
                    out->chars_before = cum;
                } else {
                    out->chars_after.back() = cum;
                }
                break;
        }
    }
    out->chars_total = cum;
    return true;
}

uint32_t wire_event_ms(const kk::msg::Event& e)
{
    return static_cast<uint32_t>(e.pre_ms) + e.hold_ms + e.gap_ms;
}

TypingPlan wire_to_plan(const kk::msg::Event* events, size_t count)
{
    TypingPlan plan;
    for (size_t i = 0; i < count; ++i) {
        const kk::msg::Event& e = events[i];
        HidEvent h;
        switch (e.kind) {
            case kk::msg::EventKind::Key: h.kind = HidEvent::Kind::Key; break;
            case kk::msg::EventKind::Pause: h.kind = HidEvent::Kind::Pause; break;
            case kk::msg::EventKind::LayoutOn: h.kind = HidEvent::Kind::LayoutOn; break;
            case kk::msg::EventKind::LayoutOff: h.kind = HidEvent::Kind::LayoutOff; break;
        }
        h.modifier = e.mods;
        h.keycode = e.usage;
        h.pre_ms = e.pre_ms;
        h.hold_ms = e.hold_ms;
        h.gap_ms = e.gap_ms;
        plan.events.push_back(h);
    }
    return plan;
}

} // namespace usb
