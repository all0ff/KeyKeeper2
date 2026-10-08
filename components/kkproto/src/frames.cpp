#include "kkproto/frames.hpp"

#include <cstring>

namespace kk::frame {

bool encode(const uint8_t* payload, size_t n, uint8_t* out, size_t cap, size_t* out_len)
{
    if (payload == nullptr || n == 0 || n > kMaxPayload || cap < kHeaderLen + n) {
        return false;
    }
    out[0] = static_cast<uint8_t>(n >> 8);
    out[1] = static_cast<uint8_t>(n & 0xFF);
    std::memcpy(out + kHeaderLen, payload, n);
    *out_len = kHeaderLen + n;
    return true;
}

size_t Reader::feed(const uint8_t* data, size_t n)
{
    if (state_ != State::NeedMore || data == nullptr) {
        return 0;
    }
    size_t used = 0;
    while (used < n && state_ == State::NeedMore) {
        if (hdr_have_ < kHeaderLen) {
            hdr_[hdr_have_++] = data[used++];
            if (hdr_have_ == kHeaderLen) {
                need_ = (static_cast<size_t>(hdr_[0]) << 8) | hdr_[1];
                have_ = 0;
                if (need_ == 0 || need_ > kMaxPayload) {
                    state_ = State::Error;
                }
            }
            continue;
        }
        const size_t take = (n - used < need_ - have_) ? (n - used) : (need_ - have_);
        std::memcpy(buf_ + have_, data + used, take);
        have_ += take;
        used += take;
        if (have_ == need_) {
            state_ = State::FrameReady;
        }
    }
    return used;
}

void Reader::next()
{
    if (state_ == State::FrameReady) {
        hdr_have_ = need_ = have_ = 0;
        state_ = State::NeedMore;
    }
}

void Reader::reset()
{
    hdr_have_ = need_ = have_ = 0;
    state_ = State::NeedMore;
}

} // namespace kk::frame
