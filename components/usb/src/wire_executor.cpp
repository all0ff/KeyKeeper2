#include "usb/wire_executor.hpp"

namespace usb {

namespace {

void wait(KeyIo& io, uint32_t ms)
{
    if (ms > 0) {
        io.delay_ms(ms);
    }
}

} // namespace

bool WireExecutor::hotkey(const kk::msg::Event& e, KeyIo& io, const char** error)
{
    wait(io, e.pre_ms);
    if (!io.send_key(e.usage, e.mods, e.hold_ms)) {
        if (error != nullptr) {
            *error = io.last_error();
        }
        return false;
    }
    wait(io, e.gap_ms);
    return true;
}

WireOutcome WireExecutor::run(const kk::msg::Event* events, size_t count, KeyIo& io)
{
    using kk::msg::EventKind;
    using kk::msg::ResultCode;
    WireOutcome out;
    bool failed = false;
    for (size_t i = 0; i < count; ++i) {
        const kk::msg::Event& e = events[i];
        if (failed) {
            if (e.kind == EventKind::LayoutOff && run_open_) {
                hotkey(e, io, &out.error); // result ignored, as in run_plan()
                run_open_ = false;
            }
            continue;
        }
        switch (e.kind) {
            case EventKind::Key:
                if (!io.ready()) {
                    out.code = ResultCode::UsbNotReady;
                    out.error = "USB not connected";
                    failed = true;
                    break;
                }
                wait(io, e.pre_ms);
                if (!io.send_key(e.usage, e.mods, e.hold_ms)) {
                    out.code = ResultCode::HidTimeout;
                    out.error = io.last_error();
                    failed = true;
                    break;
                }
                wait(io, e.gap_ms);
                break;
            case EventKind::Pause:
                wait(io, e.gap_ms);
                break;
            case EventKind::LayoutOn:
                if (!hotkey(e, io, &out.error)) {
                    out.code = ResultCode::HidTimeout;
                    failed = true;
                } else {
                    run_open_ = true;
                    open_event_ = e;
                }
                break;
            case EventKind::LayoutOff:
                hotkey(e, io, &out.error);
                run_open_ = false;
                break;
        }
        if (!failed) {
            out.done_events = i + 1;
        }
    }
    return out;
}

bool WireExecutor::close_run(KeyIo& io)
{
    if (!run_open_) {
        return false;
    }
    run_open_ = false;
    return hotkey(open_event_, io, nullptr);
}

kk::msg::Result execute_type_keys(WireExecutor& exec, const uint8_t* body, size_t n, uint16_t seq, KeyIo& io,
                                  const char** error)
{
    kk::msg::Result r;
    r.seq = seq;
    kk::msg::Event events[kk::msg::kMaxEventsPerBatch];
    size_t count = 0;
    if (kk::msg::decode_type_body(body, n, events, &count) != kk::msg::EventsStatus::Ok) {
        r.code = kk::msg::ResultCode::Rejected;
        r.done_events = 0;
        return r;
    }
    const WireOutcome o = exec.run(events, count, io);
    r.code = o.code;
    r.done_events = static_cast<uint8_t>(o.done_events);
    if (error != nullptr) {
        *error = o.error;
    }
    return r;
}

} // namespace usb
