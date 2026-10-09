#pragma once

#include "kkproto/messages.hpp"
#include "usb/typing_plan.hpp"

#include <cstddef>
#include <vector>

namespace usb {

// =============================================================================
// plan <-> wire. A TypingPlan (what to type) becomes the kk::msg::Event list the radio dongle
// executes, and back. Pure: no USB, no FreeRTOS, no link; runs and is tested on a PC.
//
// The wire event is smaller than HidEvent (6 bytes, every time field 0..255 ms), so:
//   - a wait longer than 255 ms is split into several Pause events (same total time);
//   - a key HOLD longer than 255 ms is shortened to 255 and `clamped` is set (a hold of 0 becomes 1);
//   - Skip steps (a character with no key) are not sent at all: they are only counted. The
//     "Unsupported character" note is raised by the caller from `unsupported`.
// The dongle does not know about characters. `chars_after[i]` tells the vault how many characters of
// the plan are done once wire event i has completed, so that Result.done_events can be turned
// back into RunResult::chars_sent.
// =============================================================================

struct WirePlan {
    std::vector<kk::msg::Event> events;
    /// Characters credited once events[i] has completed (cumulative).
    std::vector<size_t> chars_after;
    /// Characters credited before the first wire event (leading unsupported characters).
    size_t chars_before = 0;
    size_t chars_total = 0;
    bool unsupported = false; ///< the plan contains a character that has no key
    bool clamped = false;     ///< a hold time was changed to fit the wire format

    /// Characters done when `done_events` leading events have completed.
    size_t chars_done(size_t done_events) const
    {
        if (done_events == 0) {
            return chars_before;
        }
        if (done_events > chars_after.size()) {
            done_events = chars_after.size();
        }
        return chars_after[done_events - 1];
    }
};

/// false only if a step cannot be expressed as a valid wire event (never for plans made by plan_events()).
bool plan_to_wire(const TypingPlan& plan, WirePlan* out);

/// Total time the dongle spends on one wire event, milliseconds.
uint32_t wire_event_ms(const kk::msg::Event& e);

/// The dongle side: wire events back to the plan the local executor understands (chars stay 0).
TypingPlan wire_to_plan(const kk::msg::Event* events, size_t count);

} // namespace usb
