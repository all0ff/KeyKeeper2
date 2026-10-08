#pragma once

#include "usb/typing_plan.hpp"
#include "usb/typing_runner.hpp"

namespace usb {

// =============================================================================
// OutputSink -- where a TypingPlan goes.
//
// TypeEngine plans; an OutputSink plays. Today there is one sink, the USB
// cable (UsbCableSink, in cable_sink.cpp: the plan is executed locally through
// hid::). A radio-dongle sink is the next implementation: it will serialise the
// same plan into messages, send them over the encrypted link and report the
// dongle's RESULT as a RunResult. Nothing above this interface has to change.
// =============================================================================

class OutputSink {
public:
    virtual ~OutputSink() = default;

    /// Can the sink accept a plan right now (USB mounted by the host, ...)?
    virtual bool available() = 0;

    /// Plays the plan to completion (or to the first failure). Blocking.
    virtual RunResult play(const TypingPlan& plan) = 0;
};

/// The process-wide USB cable sink.
OutputSink& usb_cable_sink();

} // namespace usb
