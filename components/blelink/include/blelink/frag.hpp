#pragma once
// Fragmentation of application messages over BLE ATT writes/notifications.
// Pure C++ (no ESP-IDF, no NimBLE): unit-tested on the host (test_host/).
//
// One ATT value carries at most MTU-3 bytes (244 at MTU 247, only 20 at the default MTU 23).
// A message (up to kMaxMsg bytes) is cut into fragments:
//     [ id:1 ][ flags:1 ][ chunk... ]
//   id    - message number, increments per message (wraps)
//   flags - bit0 FIRST, bit1 LAST (a single-fragment message has both)
// BLE delivers in order and without loss while connected, so there is no retransmission here;
// any inconsistency (missing first fragment, id change, overflow) drops the message and is counted.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace blelink {

inline constexpr size_t kMaxMsg = 256;
inline constexpr size_t kHdr = 2;
inline constexpr uint8_t kFirst = 0x01;
inline constexpr uint8_t kLast = 0x02;

// Splits `msg` into fragments of at most `att_payload` bytes (att_payload = MTU - 3).
// Usage:  Fragmenter f(msg, n, att_payload, id);  while (f.next(buf, &len)) send(buf, len);
class Fragmenter {
public:
    Fragmenter(const uint8_t* msg, size_t n, size_t att_payload, uint8_t id)
        : msg_(msg), n_(n), id_(id), chunk_(att_payload > kHdr ? att_payload - kHdr : 0) {}

    bool done() const { return started_ && off_ >= n_; }

    // `out` must hold at least att_payload bytes. Returns false when there is nothing more
    // (or the parameters are unusable: chunk 0, message too big).
    bool next(uint8_t* out, size_t* len) {
        if (chunk_ == 0 || n_ > kMaxMsg || done()) return false;
        size_t take = n_ - off_;
        if (take > chunk_) take = chunk_;
        uint8_t fl = 0;
        if (off_ == 0) fl |= kFirst;
        if (off_ + take >= n_) fl |= kLast;
        out[0] = id_;
        out[1] = fl;
        if (take) std::memcpy(out + kHdr, msg_ + off_, take);
        off_ += take;
        started_ = true;
        *len = kHdr + take;
        return true;
    }

private:
    const uint8_t* msg_;
    size_t n_;
    uint8_t id_;
    size_t chunk_;
    size_t off_ = 0;
    bool started_ = false;  // an empty message still produces one fragment
};

struct ReasmStats {
    uint32_t messages = 0;
    uint32_t dropped = 0;  // inconsistent fragment sequences
};

class Reassembler {
public:
    // Feeds one received ATT value. Returns true when a complete message is available
    // in data()/size() (valid until the next push()).
    bool push(const uint8_t* frag, size_t len) {
        if (len < kHdr) { drop(); return false; }
        const uint8_t id = frag[0], fl = frag[1];
        const size_t body = len - kHdr;
        if (fl & kFirst) {
            if (active_) ++stats_.dropped;  // previous message never finished
            active_ = true;
            id_ = id;
            n_ = 0;
        } else if (!active_ || id != id_) {
            drop();
            return false;
        }
        if (n_ + body > kMaxMsg) { drop(); return false; }
        if (body) std::memcpy(buf_ + n_, frag + kHdr, body);
        n_ += body;
        if (fl & kLast) {
            active_ = false;
            ++stats_.messages;
            return true;
        }
        return false;
    }

    void reset() { active_ = false; n_ = 0; }
    const uint8_t* data() const { return buf_; }
    size_t size() const { return n_; }
    const ReasmStats& stats() const { return stats_; }

private:
    void drop() { active_ = false; n_ = 0; ++stats_.dropped; }

    uint8_t buf_[kMaxMsg];
    size_t n_ = 0;
    uint8_t id_ = 0;
    bool active_ = false;
    ReasmStats stats_;
};

}  // namespace blelink
