#pragma once

#include <cstdint>

// =============================================================================
// interfaces::channels -- transport_types.hpp
//
// Shared types for the channel abstractions (channel.hpp,
// input_channel.hpp, output_channel.hpp). No transport is implemented
// yet -- USB HID, Web/Wi-Fi, and BLE all come later. This only fixes
// the vocabulary so vault/gui can be written against IChannel-style
// contracts now, without depending on which transport eventually
// backs them.
// =============================================================================

namespace interfaces::channels {

enum class TransportKind : uint8_t
{
    UsbHid,
    WebWifi,
    Ble,
};

enum class ChannelState : uint8_t
{
    Disconnected,
    Connecting,
    Connected,
    Error,
};

} // namespace interfaces::channels
