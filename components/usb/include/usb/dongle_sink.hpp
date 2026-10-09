#pragma once

#include "usb/output_sink.hpp"

#include "kkproto/messages.hpp"

#include <cstddef>
#include <cstdint>

namespace usb {

// =============================================================================
// DongleSink -- the OutputSink that types through the radio dongle.
//
// It turns the plan into wire events (plan_wire.hpp), sends them in batches of at most
// kk::msg::kMaxEventsPerBatch events and kMaxBatchMs of typing time, waits for the dongle's Result after
// each batch, and maps the answer back to a RunResult. The rules of the local executor survive the trip:
//   - the first failing batch stops the run;
//   - if a Cyrillic run was opened and the failure left it open, the sink sends the closing hotkey
//     as a last batch (the dongle's executor already closed it if the closing event was in the same batch).
//
// The link itself (UART / BLE, Noise, sequence numbers) is behind DongleChannel. No ESP-IDF here.
// =============================================================================

class DongleChannel {
public:
    virtual ~DongleChannel() = default;
    /// The encrypted session is up.
    virtual bool linked() = 0;
    /// The dongle reports its USB port mounted by the host.
    virtual bool usb_ready() = 0;
    /// Sends `body` as a TypeKeys message and waits up to timeout_ms for the matching Result.
    /// false: the link was lost or nothing came back in time (the batch may or may not have run).
    virtual bool type_keys(const uint8_t* body, size_t n, kk::msg::Result* result, uint32_t timeout_ms) = 0;
};

class DongleSink final : public OutputSink {
public:
    /// Typing time of one batch. Keeps the dongle's main loop (and the link keep-alive) from stalling.
    static constexpr uint32_t kMaxBatchMs = 2500;
    /// Added to a batch's typing time to get the wait for its Result.
    static constexpr uint32_t kResultSlackMs = 3000;

    explicit DongleSink(DongleChannel& channel) : channel_(channel) {}

    bool available() override { return channel_.linked() && channel_.usb_ready(); }
    RunResult play(const TypingPlan& plan) override;

private:
    DongleChannel& channel_;
};

} // namespace usb
