#pragma once

#include "interfaces/channels/channel.hpp"

#include <cstddef>
#include <cstdint>

// =============================================================================
// interfaces::channels::InputChannel
//
// Contract for a channel the firmware receives data FROM -- e.g.
// commands or configuration arriving over Web/Wi-Fi or BLE. No
// implementation yet; see channel.hpp for why.
// =============================================================================

namespace interfaces::channels {

class InputChannel : public Channel
{
public:
    using ReceiveCallback = void (*)(const uint8_t* data, size_t len, void* ctx);

    /**
     * @brief Register a callback invoked whenever data arrives on this
     *        channel.
     *
     * Delivery context (which task calls it, whether it can block) is
     * transport-specific -- concrete implementations document it.
     * Only one callback at a time; registering a new one replaces the
     * previous.
     */
    virtual void set_receive_callback(ReceiveCallback cb, void* ctx) = 0;
};

} // namespace interfaces::channels
