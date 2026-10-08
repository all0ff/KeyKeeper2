// UART link test: frame format and stream parser.
//
// Pure C++ (no ESP-IDF), so it is unit-tested on the PC (test_host/).
//
// Frame on the wire:
//
//   0xA5 0x5A | LEN (1 byte, 0..MAX_PAYLOAD) | PAYLOAD (LEN bytes) | CRC16 (2 bytes, big endian)
//
// CRC16 is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over LEN + PAYLOAD.
//
// The parser keeps the bytes of a candidate frame in a buffer. If the CRC does not match, it drops
// ONE byte and scans the rest again, so a corrupted length byte cannot make it swallow good frames
// that follow: they are recovered from the buffer as soon as the bad candidate is finished.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace uartlink {

constexpr uint8_t SYNC0 = 0xA5;
constexpr uint8_t SYNC1 = 0x5A;
constexpr size_t MAX_PAYLOAD = 120;
constexpr size_t HEADER = 3;   // SYNC0 SYNC1 LEN
constexpr size_t TRAILER = 2;  // CRC16
constexpr size_t MAX_FRAME = HEADER + MAX_PAYLOAD + TRAILER;

inline uint16_t crc16(const uint8_t* data, size_t n, uint16_t crc = 0xFFFF)
{
    for (size_t i = 0; i < n; ++i) {
        crc = static_cast<uint16_t>(crc ^ static_cast<uint16_t>(data[i] << 8));
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                                  : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

// Writes one frame to `out`. Returns the frame size, or 0 if the payload is too long or `cap` too small.
inline size_t encode(const uint8_t* payload, size_t len, uint8_t* out, size_t cap)
{
    if (len > MAX_PAYLOAD || cap < HEADER + len + TRAILER) {
        return 0;
    }
    out[0] = SYNC0;
    out[1] = SYNC1;
    out[2] = static_cast<uint8_t>(len);
    if (len != 0) {
        std::memcpy(out + HEADER, payload, len);
    }
    const uint16_t crc = crc16(out + 2, 1 + len);
    out[HEADER + len] = static_cast<uint8_t>(crc >> 8);
    out[HEADER + len + 1] = static_cast<uint8_t>(crc & 0xFFu);
    return HEADER + len + TRAILER;
}

class Parser {
public:
    uint32_t frames_ok = 0;   // frames with a correct CRC
    uint32_t bad_crc = 0;     // candidates whose CRC did not match
    uint32_t junk_bytes = 0;  // bytes thrown away (noise before a frame, or the start of a bad candidate)

    size_t buffered() const { return n_; }

    // `on_frame(const uint8_t* payload, size_t len)` is called once per good frame, in stream order.
    // The pointer is valid only during the call.
    template <class F>
    void feed(uint8_t byte, F&& on_frame)
    {
        buf_[n_++] = byte;  // safe: scan() always leaves n_ < MAX_FRAME
        scan(on_frame);
    }

    template <class F>
    void feed(const uint8_t* data, size_t n, F&& on_frame)
    {
        for (size_t i = 0; i < n; ++i) {
            feed(data[i], on_frame);
        }
    }

private:
    uint8_t buf_[MAX_FRAME] = {};
    size_t n_ = 0;

    void drop(size_t k)
    {
        std::memmove(buf_, buf_ + k, n_ - k);
        n_ -= k;
    }

    template <class F>
    void scan(F& on_frame)
    {
        for (;;) {
            if (n_ == 0) {
                return;
            }
            if (buf_[0] != SYNC0) {
                ++junk_bytes;
                drop(1);
                continue;
            }
            if (n_ < 2) {
                return;
            }
            if (buf_[1] != SYNC1) {
                ++junk_bytes;
                drop(1);
                continue;
            }
            if (n_ < 3) {
                return;
            }
            const size_t len = buf_[2];
            if (len > MAX_PAYLOAD) {
                ++junk_bytes;
                drop(1);
                continue;
            }
            const size_t total = HEADER + len + TRAILER;
            if (n_ < total) {
                return;
            }
            const uint16_t got = static_cast<uint16_t>((buf_[HEADER + len] << 8) | buf_[HEADER + len + 1]);
            if (crc16(buf_ + 2, 1 + len) == got) {
                ++frames_ok;
                on_frame(static_cast<const uint8_t*>(buf_ + HEADER), len);
                drop(total);
                continue;
            }
            ++bad_crc;
            ++junk_bytes;
            drop(1);
        }
    }
};

}  // namespace uartlink
