#pragma once

#include <cstddef>
#include <cstdint>

namespace kk::msg {

// =============================================================================
// Application messages carried inside the Noise transport (the plaintext).
//
//   type(1) | flags(1) | seq(2, LE) | len(2, LE) | body(len)
//
// Pure encode / decode with bounds checks on every length: nothing here trusts
// the peer. See the protocol specification for the meaning of each message.
// =============================================================================

constexpr uint8_t kProtocolVersion = 1;
constexpr size_t kHeaderLen = 6;
/// One message must fit one 247-byte GATT write: 244 - frame header 2 - AEAD tag 16 - header 6.
constexpr size_t kMaxBody = 220;

enum class Type : uint8_t {
    Hello = 1,       ///< vault -> dongle: version, capabilities
    HelloAck = 2,    ///< dongle -> vault: version, firmware, USB state
    TypeKeys = 3,    ///< vault -> dongle: a batch of events to type ("TYPE")
    Result = 4,      ///< dongle -> vault: outcome of a TypeKeys
    State = 5,       ///< dongle -> vault: USB / HID state changed
    Abort = 6,       ///< vault -> dongle: stop the running batch
    Ping = 7,
    Pong = 8,
    Bye = 9,
    PairConfirm = 10, ///< vault -> dongle during pairing: the user accepted the code
    Language = 11     ///< vault -> dongle: 1 byte, the vault's menu language (kLangEnglish / kLangRussian)
};

struct Header {
    Type type = Type::Ping;
    uint8_t flags = 0;
    uint16_t seq = 0;
    uint16_t len = 0;
};

enum class ParseStatus : uint8_t { Ok, Truncated, BadLength, UnknownType };

bool encode(Type type, uint8_t flags, uint16_t seq, const uint8_t* body, size_t body_len, uint8_t* out, size_t cap,
            size_t* out_len);

/// body points into |in|. The length field must match exactly (no trailing bytes).
ParseStatus parse(const uint8_t* in, size_t n, Header* header, const uint8_t** body);

// ----------------------------------------------------------------------------- events

enum class EventKind : uint8_t {
    Key = 1,       ///< press mods+usage for hold_ms after waiting pre_ms, then wait gap_ms
    Pause = 2,     ///< wait gap_ms
    LayoutOn = 3,  ///< layout-switch hotkey opening a Cyrillic run
    LayoutOff = 4, ///< the hotkey closing it (still executed after a failure inside the run)
};

constexpr size_t kEventLen = 6;
constexpr size_t kMaxEventsPerBatch = 32; ///< 1 + 6 * 32 = 193 bytes of body

struct Event {
    EventKind kind = EventKind::Key;
    uint8_t mods = 0;
    uint8_t usage = 0;
    uint8_t pre_ms = 0;
    uint8_t hold_ms = 0;
    uint8_t gap_ms = 0;
};

/// The dongle's own acceptance rules, enforced here so they are the same everywhere:
/// Key: keyboard-page usage 0x04-0xE7 and hold >= 1 ms; Pause: only gap_ms used;
/// Layout hotkeys: modifiers only (usage 0, mods != 0), hold >= 1 ms.
bool event_valid(const Event& e);

bool encode_type_body(const Event* events, size_t count, uint8_t* out, size_t cap, size_t* out_len);

enum class EventsStatus : uint8_t { Ok, Truncated, TooMany, Empty, BadEvent };
EventsStatus decode_type_body(const uint8_t* body, size_t n, Event out[kMaxEventsPerBatch], size_t* count);

// ----------------------------------------------------------------------------- small fixed bodies

struct Hello {
    uint8_t version = kProtocolVersion;
    uint8_t caps = 0;
};
struct HelloAck {
    uint8_t version = kProtocolVersion;
    uint8_t fw_major = 0;
    uint8_t fw_minor = 0;
    bool usb_mounted = false;
};

constexpr uint8_t kLangEnglish = 0;
constexpr uint8_t kLangRussian = 1;

constexpr uint8_t kStateUsbMounted = 0x01;
constexpr uint8_t kStateHidReady = 0x02;
constexpr uint8_t kStateBusy = 0x04;

enum class ResultCode : uint8_t { Ok = 0, Busy = 1, UsbNotReady = 2, HidTimeout = 3, Rejected = 4, TooLong = 5 };
struct Result {
    uint16_t seq = 0;       ///< seq of the TypeKeys message this answers
    ResultCode code = ResultCode::Ok;
    uint8_t done_events = 0; ///< events completed (the vault maps it back to characters)
};

bool encode_hello(const Hello& v, uint8_t out[2]);
bool decode_hello(const uint8_t* body, size_t n, Hello* v);
bool encode_hello_ack(const HelloAck& v, uint8_t out[4]);
bool decode_hello_ack(const uint8_t* body, size_t n, HelloAck* v);
bool encode_result(const Result& v, uint8_t out[4]);
bool decode_result(const uint8_t* body, size_t n, Result* v); ///< rejects unknown result codes
bool encode_abort(uint16_t seq, uint8_t out[2]);
bool decode_abort(const uint8_t* body, size_t n, uint16_t* seq);

} // namespace kk::msg
