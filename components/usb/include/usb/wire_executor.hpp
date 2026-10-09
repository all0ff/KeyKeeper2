#pragma once

#include "kkproto/messages.hpp"
#include "usb/typing_runner.hpp"

#include <cstddef>
#include <cstdint>

namespace usb {

// =============================================================================
// The dongle's executor for TypeKeys batches. It follows exactly the rules of run_plan() (the local
// executor), applied to wire events, and remembers across batches whether a Cyrillic run is open:
//
//   - before every Key: if !io.ready() the batch stops, code UsbNotReady;
//   - the first failing step stops the batch (HidTimeout for a key that could not be sent);
//   - after a stop, LayoutOff events LATER IN THE SAME BATCH are still sent if a run is open
//     (the host must not stay on the wrong layout); LayoutOn / Key / Pause are dropped;
//   - done_events = number of leading events that completed successfully.
//
// A run that is still open when the batch ends (its LayoutOff is in a later batch) stays open. The
// vault closes it by sending that LayoutOff; if the link dies instead, the dongle calls close_run().
// No ESP-IDF, no link: tested on a PC.
// =============================================================================

struct WireOutcome {
    kk::msg::ResultCode code = kk::msg::ResultCode::Ok;
    size_t done_events = 0;
    const char* error = nullptr; ///< io.last_error() of the failure, for the log
};

class WireExecutor {
public:
    WireOutcome run(const kk::msg::Event* events, size_t count, KeyIo& io);

    bool run_open() const { return run_open_; }

    /// The link is gone: close a Cyrillic run that was left open. true if the hotkey was sent.
    bool close_run(KeyIo& io);

private:
    bool hotkey(const kk::msg::Event& e, KeyIo& io, const char** error);

    bool run_open_ = false;
    kk::msg::Event open_event_; ///< the LayoutOn that opened the run (same hotkey closes it)
};

/// Decodes a TypeKeys body, runs it and returns the Result to send back (seq = the TypeKeys seq).
/// A malformed body is Rejected without touching the keyboard.
kk::msg::Result execute_type_keys(WireExecutor& exec, const uint8_t* body, size_t n, uint16_t seq, KeyIo& io,
                                  const char** error = nullptr);

} // namespace usb
