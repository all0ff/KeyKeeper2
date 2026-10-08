// UART link test: ping payload, per-peer sequence accounting and the one-line verdict.
// Pure C++ (no ESP-IDF), unit-tested on the PC.
#pragma once

#include <cstddef>
#include <cstdint>

#include "link_frame.hpp"

namespace uartlink {

constexpr size_t PING_MIN = 10;               // id(2) + seq(4) + echo(4)
constexpr uint32_t NO_ECHO = 0xFFFFFFFFu;     // "I have not heard the peer yet"

struct Ping {
    uint16_t id = 0;      // sender identity (last two bytes of its MAC): tells the two boards apart
    uint32_t seq = 0;     // sender's frame counter
    uint32_t echo = NO_ECHO;  // last `seq` the sender has received from the other side
};

// Padding bytes are a function of (seq, index), so the receiver can check them: this exercises the
// real wire with varying data (it deliberately produces 0xA5 0x5A pairs now and then).
inline uint8_t fill_byte(uint32_t seq, size_t i)
{
    return static_cast<uint8_t>(seq * 31u + static_cast<uint32_t>(i) * 17u + 0xA5u);
}

inline void put_u32(uint8_t* p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

inline uint32_t get_u32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// Fills `out` (at least payload_len bytes). Returns payload_len, or 0 if the length is not allowed.
inline size_t encode_ping(const Ping& p, size_t payload_len, uint8_t* out)
{
    if (payload_len < PING_MIN || payload_len > MAX_PAYLOAD) {
        return 0;
    }
    out[0] = static_cast<uint8_t>(p.id & 0xFFu);
    out[1] = static_cast<uint8_t>(p.id >> 8);
    put_u32(out + 2, p.seq);
    put_u32(out + 6, p.echo);
    for (size_t i = PING_MIN; i < payload_len; ++i) {
        out[i] = fill_byte(p.seq, i);
    }
    return payload_len;
}

enum class PingResult { Ok, TooShort, BadFill };

inline PingResult decode_ping(const uint8_t* d, size_t len, Ping& p)
{
    if (len < PING_MIN) {
        return PingResult::TooShort;
    }
    p.id = static_cast<uint16_t>(d[0] | (d[1] << 8));
    p.seq = get_u32(d + 2);
    p.echo = get_u32(d + 6);
    for (size_t i = PING_MIN; i < len; ++i) {
        if (d[i] != fill_byte(p.seq, i)) {
            return PingResult::BadFill;
        }
    }
    return PingResult::Ok;
}

// What we know about the other board.
struct PeerTracker {
    bool have = false;
    uint16_t id = 0;
    uint32_t last_seq = 0;
    uint32_t last_echo = NO_ECHO;
    uint32_t ok = 0;        // pings received from the peer
    uint32_t lost = 0;      // sequence numbers that never arrived (gaps)
    uint32_t restarts = 0;  // peer rebooted (its counter went back) or a different board appeared

    void on_ping(const Ping& p)
    {
        if (!have) {
            have = true;
            id = p.id;
        } else if (p.id != id) {
            id = p.id;
            ++restarts;
        } else if (p.seq == last_seq + 1u) {
            // normal
        } else if (p.seq > last_seq + 1u) {
            lost += p.seq - last_seq - 1u;
        } else {
            ++restarts;  // UART cannot reorder, so seq <= last means the peer restarted
        }
        last_seq = p.seq;
        last_echo = p.echo;
        ++ok;
    }

    // Does the peer hear us? `next_seq` is the seq of the next frame we will send. The peer echoes the
    // last of our frames it received; if that is recent, our TX wire works.
    bool sees_me(uint32_t next_seq, uint32_t window = 25) const
    {
        if (!have || last_echo == NO_ECHO || next_seq == 0) {
            return false;
        }
        const uint32_t last_sent = next_seq - 1u;
        return last_sent >= last_echo && (last_sent - last_echo) <= window;
    }
};

enum class Verdict { Loopback, NoData, Garbage, OneWay, Ok };

// Decides from what happened in the last second.
inline Verdict classify(uint32_t own_frames_delta, uint32_t rx_bytes_delta, uint32_t rx_ok_delta, bool peer_sees_me)
{
    if (own_frames_delta > 0) {
        return Verdict::Loopback;
    }
    if (rx_bytes_delta == 0) {
        return Verdict::NoData;
    }
    if (rx_ok_delta == 0) {
        return Verdict::Garbage;
    }
    if (!peer_sees_me) {
        return Verdict::OneWay;
    }
    return Verdict::Ok;
}

inline const char* verdict_tag(Verdict v)
{
    switch (v) {
    case Verdict::Loopback: return "LOOPBACK";
    case Verdict::NoData:   return "NO DATA";
    case Verdict::Garbage:  return "GARBAGE";
    case Verdict::OneWay:   return "ONE WAY";
    case Verdict::Ok:       return "LINK OK";
    }
    return "?";
}

inline const char* verdict_hint(Verdict v)
{
    switch (v) {
    case Verdict::Loopback:
        return "I receive my own frames: TX is wired to RX on this board. The pins work. "
               "Remove the jumper and connect the other board.";
    case Verdict::NoData:
        return "Nothing arrives on RX. Check that TX and RX are crossed (TX of one board to RX of the other), "
               "that GND is shared, and that the other board runs this test.";
    case Verdict::Garbage:
        return "Bytes arrive but no valid frame. Check that both boards use the same baud rate, that TX is not "
               "wired to TX, and that the wires are short.";
    case Verdict::OneWay:
        return "I hear the other board, but it does not hear me. Check the wire from my TX to its RX.";
    case Verdict::Ok:
        return "Both directions work.";
    }
    return "";
}

}  // namespace uartlink
