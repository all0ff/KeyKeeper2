#pragma once

#include "interfaces/channels/transport_types.hpp"

// =============================================================================
// interfaces::channels::Channel
//
// Base contract every transport channel implements, regardless of
// direction (see InputChannel/OutputChannel for the direction-
// specific parts). Purely abstract -- no concrete transport exists
// yet. The point of defining this now is so vault/gui code can be
// written against Channel/InputChannel/OutputChannel from the start,
// and a real USB HID / Web / BLE implementation can be dropped in
// later without touching vault/gui at all.
// =============================================================================

namespace interfaces::channels {

class Channel
{
public:
    virtual ~Channel() = default;

    virtual TransportKind kind() const = 0;

    /// Short human-readable name, e.g. "USB HID". For logging/UI, not
    /// meant to be parsed.
    virtual const char* name() const = 0;

    virtual ChannelState state() const = 0;

    /// Establish the channel (e.g. start advertising for BLE, wait for
    /// USB enumeration). Exact semantics are transport-specific.
    virtual bool open() = 0;

    virtual void close() = 0;
};

} // namespace interfaces::channels
