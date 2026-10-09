#include "kkproto/messages.hpp"

#include <cstring>

namespace kk::msg {

namespace {
bool known_type(uint8_t t)
{
    return t >= static_cast<uint8_t>(Type::Hello) && t <= static_cast<uint8_t>(Type::Language);
}
void put_le16(uint8_t* p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>(v >> 8);
}
uint16_t get_le16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
} // namespace

bool encode(Type type, uint8_t flags, uint16_t seq, const uint8_t* body, size_t body_len, uint8_t* out, size_t cap,
            size_t* out_len)
{
    if (!known_type(static_cast<uint8_t>(type)) || body_len > kMaxBody || cap < kHeaderLen + body_len ||
        (body == nullptr && body_len != 0)) {
        return false;
    }
    out[0] = static_cast<uint8_t>(type);
    out[1] = flags;
    put_le16(out + 2, seq);
    put_le16(out + 4, static_cast<uint16_t>(body_len));
    if (body_len > 0) {
        std::memcpy(out + kHeaderLen, body, body_len);
    }
    *out_len = kHeaderLen + body_len;
    return true;
}

ParseStatus parse(const uint8_t* in, size_t n, Header* header, const uint8_t** body)
{
    if (in == nullptr || n < kHeaderLen) {
        return ParseStatus::Truncated;
    }
    if (!known_type(in[0])) {
        return ParseStatus::UnknownType;
    }
    const uint16_t len = get_le16(in + 4);
    if (len > kMaxBody || static_cast<size_t>(len) != n - kHeaderLen) {
        return ParseStatus::BadLength;
    }
    header->type = static_cast<Type>(in[0]);
    header->flags = in[1];
    header->seq = get_le16(in + 2);
    header->len = len;
    *body = in + kHeaderLen;
    return ParseStatus::Ok;
}

bool event_valid(const Event& e)
{
    switch (e.kind) {
        case EventKind::Key: return e.usage >= 0x04 && e.usage <= 0xE7 && e.hold_ms >= 1;
        case EventKind::Pause: return e.mods == 0 && e.usage == 0 && e.pre_ms == 0 && e.hold_ms == 0;
        case EventKind::LayoutOn:
        case EventKind::LayoutOff: return e.usage == 0 && e.mods != 0 && e.hold_ms >= 1;
    }
    return false;
}

bool encode_type_body(const Event* events, size_t count, uint8_t* out, size_t cap, size_t* out_len)
{
    if (count == 0 || count > kMaxEventsPerBatch || events == nullptr || cap < 1 + count * kEventLen) {
        return false;
    }
    out[0] = static_cast<uint8_t>(count);
    for (size_t i = 0; i < count; ++i) {
        if (!event_valid(events[i])) {
            return false;
        }
        uint8_t* p = out + 1 + i * kEventLen;
        p[0] = static_cast<uint8_t>(events[i].kind);
        p[1] = events[i].mods;
        p[2] = events[i].usage;
        p[3] = events[i].pre_ms;
        p[4] = events[i].hold_ms;
        p[5] = events[i].gap_ms;
    }
    *out_len = 1 + count * kEventLen;
    return true;
}

EventsStatus decode_type_body(const uint8_t* body, size_t n, Event out[kMaxEventsPerBatch], size_t* count)
{
    if (body == nullptr || n < 1) {
        return EventsStatus::Truncated;
    }
    const size_t c = body[0];
    if (c == 0) {
        return EventsStatus::Empty;
    }
    if (c > kMaxEventsPerBatch) {
        return EventsStatus::TooMany;
    }
    if (n != 1 + c * kEventLen) {
        return EventsStatus::Truncated;
    }
    for (size_t i = 0; i < c; ++i) {
        const uint8_t* p = body + 1 + i * kEventLen;
        Event e;
        if (p[0] < static_cast<uint8_t>(EventKind::Key) || p[0] > static_cast<uint8_t>(EventKind::LayoutOff)) {
            return EventsStatus::BadEvent;
        }
        e.kind = static_cast<EventKind>(p[0]);
        e.mods = p[1];
        e.usage = p[2];
        e.pre_ms = p[3];
        e.hold_ms = p[4];
        e.gap_ms = p[5];
        if (!event_valid(e)) {
            return EventsStatus::BadEvent;
        }
        out[i] = e;
    }
    *count = c;
    return EventsStatus::Ok;
}

bool encode_hello(const Hello& v, uint8_t out[2])
{
    out[0] = v.version;
    out[1] = v.caps;
    return true;
}
bool decode_hello(const uint8_t* body, size_t n, Hello* v)
{
    if (body == nullptr || n != 2) {
        return false;
    }
    v->version = body[0];
    v->caps = body[1];
    return true;
}
bool encode_hello_ack(const HelloAck& v, uint8_t out[4])
{
    out[0] = v.version;
    out[1] = v.fw_major;
    out[2] = v.fw_minor;
    out[3] = v.usb_mounted ? 1 : 0;
    return true;
}
bool decode_hello_ack(const uint8_t* body, size_t n, HelloAck* v)
{
    if (body == nullptr || n != 4 || body[3] > 1) {
        return false;
    }
    v->version = body[0];
    v->fw_major = body[1];
    v->fw_minor = body[2];
    v->usb_mounted = body[3] == 1;
    return true;
}
bool encode_result(const Result& v, uint8_t out[4])
{
    put_le16(out, v.seq);
    out[2] = static_cast<uint8_t>(v.code);
    out[3] = v.done_events;
    return true;
}
bool decode_result(const uint8_t* body, size_t n, Result* v)
{
    if (body == nullptr || n != 4 || body[2] > static_cast<uint8_t>(ResultCode::TooLong)) {
        return false;
    }
    v->seq = get_le16(body);
    v->code = static_cast<ResultCode>(body[2]);
    v->done_events = body[3];
    return true;
}
bool encode_abort(uint16_t seq, uint8_t out[2])
{
    put_le16(out, seq);
    return true;
}
bool decode_abort(const uint8_t* body, size_t n, uint16_t* seq)
{
    if (body == nullptr || n != 2) {
        return false;
    }
    *seq = get_le16(body);
    return true;
}

} // namespace kk::msg
