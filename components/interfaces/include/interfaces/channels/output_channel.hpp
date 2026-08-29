#pragma once

#include "interfaces/channels/channel.hpp"

#include <cstddef>
#include <cstdint>

// =============================================================================
// interfaces::channels::OutputChannel
//
// Contract for a channel the firmware sends data OUT over -- e.g. USB
// HID keystrokes (typing a password/TOTP code), or a value pushed to
// a Web UI. No implementation yet; see channel.hpp for why.
// =============================================================================

namespace interfaces::channels {

class OutputChannel : public Channel
{
public:
    /**
     * @brief Send a block of bytes out over this channel.
     *
     * Interpretation of data/len is transport-specific (e.g. HID
     * keycodes vs. a structured payload) -- concrete implementations
     * document their own meaning.
     *
     * @return true if the data was accepted for sending. Does not
     *         necessarily mean delivery is confirmed.
     */
    virtual bool send(const uint8_t* data, size_t len) = 0;
};

} // namespace interfaces::channels
