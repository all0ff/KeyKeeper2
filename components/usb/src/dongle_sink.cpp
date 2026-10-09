#include "usb/dongle_sink.hpp"

#include "usb/plan_wire.hpp"

#include <vector>

namespace usb {

namespace {

using kk::msg::Event;
using kk::msg::EventKind;
using kk::msg::ResultCode;

constexpr char ERR_NO_DONGLE[] = "Dongle not connected";
constexpr char ERR_NO_REPLY[] = "Dongle did not answer";
constexpr char ERR_NOT_CONNECTED[] = "USB not connected";
constexpr char ERR_HID[] = "HID error";
constexpr char ERR_BUSY[] = "Dongle busy";
constexpr char ERR_REJECTED[] = "Dongle rejected the keys";
constexpr char ERR_UNSUPPORTED[] = "Unsupported character";
constexpr char ERR_INTERNAL[] = "Cannot send this text";

const char* error_text(ResultCode c)
{
    switch (c) {
        case ResultCode::Ok: return nullptr;
        case ResultCode::Busy: return ERR_BUSY;
        case ResultCode::UsbNotReady: return ERR_NOT_CONNECTED;
        case ResultCode::HidTimeout: return ERR_HID;
        case ResultCode::Rejected: return ERR_REJECTED;
        case ResultCode::TooLong: return ERR_REJECTED;
    }
    return ERR_REJECTED;
}

bool send_batch(DongleChannel& ch, const Event* ev, size_t n, kk::msg::Result* r, uint32_t* ms_out)
{
    uint8_t body[1 + kk::msg::kMaxEventsPerBatch * kk::msg::kEventLen];
    size_t len = 0;
    if (!kk::msg::encode_type_body(ev, n, body, sizeof body, &len)) {
        return false;
    }
    uint32_t ms = 0;
    for (size_t i = 0; i < n; ++i) {
        ms += wire_event_ms(ev[i]);
    }
    if (ms_out != nullptr) {
        *ms_out = ms;
    }
    return ch.type_keys(body, len, r, ms + DongleSink::kResultSlackMs);
}

} // namespace

RunResult DongleSink::play(const TypingPlan& plan)
{
    RunResult res;
    WirePlan wire;
    if (!plan_to_wire(plan, &wire)) {
        res.ok = false;
        res.error = ERR_INTERNAL;
        return res;
    }
    if (wire.events.empty()) {
        // Nothing to press (empty text, or only characters without a key). The local executor still
        // refuses to run on an unusable output when it reaches a Skip step.
        if (wire.chars_total > 0 && !available()) {
            res.ok = false;
            res.error = channel_.linked() ? ERR_NOT_CONNECTED : ERR_NO_DONGLE;
            return res;
        }
        res.chars_sent = wire.chars_total;
        res.error = wire.unsupported ? ERR_UNSUPPORTED : nullptr;
        return res;
    }
    if (!channel_.linked()) {
        res.ok = false;
        res.error = ERR_NO_DONGLE;
        return res;
    }

    const size_t total = wire.events.size();
    size_t base = 0;
    bool run_open = false;     // a Cyrillic run is open on the dongle (tracked from the results)
    bool failed = false;
    const char* error = wire.unsupported ? ERR_UNSUPPORTED : nullptr;
    size_t done_total = 0;

    while (base < total) {
        // Batch: up to 32 events and about kMaxBatchMs of typing time (at least one event).
        size_t n = 0;
        uint32_t ms = 0;
        while (base + n < total && n < kk::msg::kMaxEventsPerBatch) {
            const uint32_t em = wire_event_ms(wire.events[base + n]);
            if (n > 0 && ms + em > kMaxBatchMs) {
                break;
            }
            ms += em;
            ++n;
        }

        kk::msg::Result r;
        if (!send_batch(channel_, &wire.events[base], n, &r, nullptr)) {
            // Lost link or no answer: the dongle may have typed part of it. Count what is certain.
            res.ok = false;
            res.chars_sent = wire.chars_done(done_total);
            res.error = channel_.linked() ? ERR_NO_REPLY : ERR_NO_DONGLE;
            return res;
        }

        size_t done = r.done_events;
        if (done > n) {
            done = n;
        }
        // Track the Cyrillic run over the events that completed.
        for (size_t i = 0; i < done; ++i) {
            const EventKind k = wire.events[base + i].kind;
            if (k == EventKind::LayoutOn) {
                run_open = true;
            } else if (k == EventKind::LayoutOff) {
                run_open = false;
            }
        }
        done_total = base + done;

        if (r.code != ResultCode::Ok) {
            failed = true;
            error = error_text(r.code);
            // The dongle drops the rest of the batch except LayoutOff of an open run.
            for (size_t i = done + 1; i < n; ++i) {
                if (wire.events[base + i].kind == EventKind::LayoutOff && run_open) {
                    run_open = false;
                }
            }
            if (run_open) {
                // The closing hotkey is the next LayoutOff after this batch.
                for (size_t i = base + n; i < total; ++i) {
                    if (wire.events[i].kind == EventKind::LayoutOff) {
                        kk::msg::Result ignored;
                        send_batch(channel_, &wire.events[i], 1, &ignored, nullptr);
                        break;
                    }
                }
            }
            break;
        }
        base += n;
    }

    res.chars_sent = wire.chars_done(failed ? done_total : total);
    res.ok = !failed;
    res.error = error;
    return res;
}

} // namespace usb
