#pragma once

#include <cstddef>
#include <cstdint>

namespace kk::frame {

// =============================================================================
// Length-prefixed framing for the BLE byte stream.
//
// A Noise message is sent as 2 length bytes (big-endian) followed by the
// message, as a stream over consecutive GATT writes. BLE already delivers
// reliably and in order inside a connection, but it cuts the stream at MTU
// boundaries: the Reader reassembles a frame from arbitrary chunks.
// =============================================================================

constexpr size_t kHeaderLen = 2;
constexpr size_t kMaxPayload = 512; ///< hard cap on one frame; larger lengths are an error

/// Writes the 2-byte length and the payload. false if the payload is empty or
/// larger than kMaxPayload, or out is too small. out_len = kHeaderLen + n.
bool encode(const uint8_t* payload, size_t n, uint8_t* out, size_t cap, size_t* out_len);

class Reader {
public:
    enum class State : uint8_t {
        NeedMore,   ///< waiting for more bytes
        FrameReady, ///< payload()/payload_len() are valid; call next() to continue
        Error,      ///< a bad length was seen; sticky until reset()
    };

    /// Consumes bytes of |data| until one complete frame is buffered or the
    /// input ends; returns how many bytes were consumed. If a frame is ready
    /// (or after an error) nothing is consumed: call next() / reset() first.
    size_t feed(const uint8_t* data, size_t n);

    State state() const { return state_; }
    const uint8_t* payload() const { return buf_; }
    size_t payload_len() const { return need_; }

    /// Drops the ready frame and starts on the next one.
    void next();
    void reset();

private:
    uint8_t buf_[kMaxPayload] = {};
    uint8_t hdr_[kHeaderLen] = {};
    size_t hdr_have_ = 0;
    size_t need_ = 0;
    size_t have_ = 0;
    State state_ = State::NeedMore;
};

} // namespace kk::frame
