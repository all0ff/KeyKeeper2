// Codecs: framing, messages, events. Exact-behaviour tests plus property-based fuzzing under
// ASan/UBSan: nothing may crash, and whatever a decoder accepts must re-encode to the same bytes.
#include "kkproto/frames.hpp"
#include "kkproto/messages.hpp"
#include "kkproto/noise.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstring>

using namespace kk;

struct Rng {
    uint64_t s;
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return static_cast<uint32_t>(s >> 11); }
    uint32_t below(uint32_t n) { return n ? next() % n : 0; }
    uint8_t byte() { return static_cast<uint8_t>(next()); }
};

// ---------------------------------------------------------------- exact behaviour
static void test_frames()
{
    uint8_t out[600];
    size_t n = 0;
    const uint8_t p[] = {1, 2, 3};
    CHECK(frame::encode(p, 3, out, sizeof out, &n) && n == 5 && out[0] == 0 && out[1] == 3 && out[2] == 1);
    CHECK(!frame::encode(p, 0, out, sizeof out, &n));
    CHECK(!frame::encode(nullptr, 3, out, sizeof out, &n));
    CHECK(!frame::encode(p, 3, out, 4, &n));
    Bytes big(frame::kMaxPayload, 7);
    CHECK(frame::encode(big.data(), big.size(), out, sizeof out, &n) && n == frame::kMaxPayload + 2);
    Bytes toobig(frame::kMaxPayload + 1, 7);
    uint8_t huge[600];
    CHECK(!frame::encode(toobig.data(), toobig.size(), huge, sizeof huge, &n));

    // every payload size x every chunk size reassembles
    Rng r{12345};
    for (size_t len = 1; len <= frame::kMaxPayload; len += (len < 40 ? 1 : 37)) {
        Bytes payload(len);
        for (auto& b : payload) b = r.byte();
        uint8_t wire[600];
        size_t wn = 0;
        CHECK(frame::encode(payload.data(), len, wire, sizeof wire, &wn));
        for (size_t chunk : {1u, 2u, 5u, 20u, 244u, 600u}) {
            frame::Reader rd;
            size_t off = 0;
            while (off < wn && rd.state() == frame::Reader::State::NeedMore) off += rd.feed(wire + off, std::min<size_t>(chunk, wn - off));
            CHECK(rd.state() == frame::Reader::State::FrameReady && off == wn && rd.payload_len() == len &&
                  std::memcmp(rd.payload(), payload.data(), len) == 0);
        }
    }
    // two frames back to back in one buffer: feed stops at the first frame's end
    uint8_t two[40];
    size_t t1 = 0, t2 = 0;
    const uint8_t a[] = {9, 9}, b[] = {8, 8, 8};
    CHECK(frame::encode(a, 2, two, sizeof two, &t1) && frame::encode(b, 3, two + t1, sizeof two - t1, &t2));
    frame::Reader rd;
    size_t used = rd.feed(two, t1 + t2);
    CHECK(used == t1 && rd.state() == frame::Reader::State::FrameReady && rd.payload_len() == 2);
    CHECK(rd.feed(two + used, 5) == 0);                       // must call next() first
    rd.next();
    used += rd.feed(two + used, t2);
    CHECK(used == t1 + t2 && rd.state() == frame::Reader::State::FrameReady && rd.payload_len() == 3 && rd.payload()[0] == 8);
    // bad lengths: zero and too large are errors, sticky until reset
    frame::Reader bad1, bad2;
    const uint8_t zero[] = {0, 0, 1};
    CHECK(bad1.feed(zero, 3) == 2 && bad1.state() == frame::Reader::State::Error && bad1.feed(zero, 3) == 0);
    bad1.reset();
    CHECK(bad1.state() == frame::Reader::State::NeedMore);
    const uint8_t large[] = {0x02, 0x01}; // 513
    CHECK(bad2.feed(large, 2) == 2 && bad2.state() == frame::Reader::State::Error);
}

static void test_messages()
{
    uint8_t buf[300];
    size_t n = 0;
    msg::Header h; const uint8_t* body = nullptr;
    for (int t = 1; t <= 10; ++t) {
        const uint8_t b[] = {1, 2, 3};
        CHECK(msg::encode(static_cast<msg::Type>(t), 0xA5, 0xBEEF, b, 3, buf, sizeof buf, &n) && n == 9);
        CHECK(msg::parse(buf, n, &h, &body) == msg::ParseStatus::Ok && static_cast<int>(h.type) == t && h.flags == 0xA5 && h.seq == 0xBEEF && h.len == 3 && body == buf + 6);
    }
    CHECK(msg::encode(msg::Type::Ping, 0, 0, nullptr, 0, buf, sizeof buf, &n) && n == 6);
    CHECK(msg::parse(buf, n, &h, &body) == msg::ParseStatus::Ok && h.len == 0);
    Bytes max(msg::kMaxBody, 5);
    CHECK(msg::encode(msg::Type::TypeKeys, 0, 1, max.data(), max.size(), buf, sizeof buf, &n) && n == 6 + 220);
    Bytes over(msg::kMaxBody + 1, 5);
    CHECK(!msg::encode(msg::Type::TypeKeys, 0, 1, over.data(), over.size(), buf, sizeof buf, &n));
    CHECK(!msg::encode(static_cast<msg::Type>(0), 0, 0, nullptr, 0, buf, sizeof buf, &n));
    CHECK(!msg::encode(static_cast<msg::Type>(11), 0, 0, nullptr, 0, buf, sizeof buf, &n));
    CHECK(!msg::encode(msg::Type::Ping, 0, 0, nullptr, 0, buf, 5, &n));
    // parse rejects: short, wrong length either way, unknown type, length above the cap
    uint8_t m[40];
    CHECK(msg::encode(msg::Type::Result, 0, 1, max.data(), 4, m, sizeof m, &n) && n == 10);
    CHECK(msg::parse(m, 5, &h, &body) == msg::ParseStatus::Truncated);
    CHECK(msg::parse(m, n - 1, &h, &body) == msg::ParseStatus::BadLength);
    m[n] = 0;
    CHECK(msg::parse(m, n + 1, &h, &body) == msg::ParseStatus::BadLength);
    for (uint8_t t : {0, 11, 255}) { uint8_t c[6] = {t, 0, 0, 0, 0, 0}; CHECK(msg::parse(c, 6, &h, &body) == msg::ParseStatus::UnknownType); }
    uint8_t big[6 + 221] = {3, 0, 0, 0, 221, 0};
    CHECK(msg::parse(big, sizeof big, &h, &body) == msg::ParseStatus::BadLength);
    CHECK(msg::parse(nullptr, 0, &h, &body) == msg::ParseStatus::Truncated);
}

static void test_events()
{
    using msg::Event; using msg::EventKind;
    // validity matrix
    CHECK(msg::event_valid({EventKind::Key, 0, 0x04, 0, 1, 0}));
    CHECK(msg::event_valid({EventKind::Key, 0x02, 0xE7, 0, 255, 255}));
    CHECK(!msg::event_valid({EventKind::Key, 0, 0x03, 0, 10, 10}));   // below the keyboard page
    CHECK(!msg::event_valid({EventKind::Key, 0, 0xE8, 0, 10, 10}));   // above it
    CHECK(!msg::event_valid({EventKind::Key, 0, 0x04, 0, 0, 10}));    // hold must be >= 1
    CHECK(!msg::event_valid({EventKind::Key, 0, 0x00, 0, 10, 10}));
    CHECK(msg::event_valid({EventKind::Pause, 0, 0, 0, 0, 200}));
    CHECK(!msg::event_valid({EventKind::Pause, 0x02, 0, 0, 0, 200}));
    CHECK(!msg::event_valid({EventKind::Pause, 0, 0x10, 0, 0, 200}));
    CHECK(!msg::event_valid({EventKind::Pause, 0, 0, 5, 0, 200}));
    CHECK(msg::event_valid({EventKind::LayoutOn, 0x06, 0, 60, 50, 80}));
    CHECK(msg::event_valid({EventKind::LayoutOff, 0x06, 0, 60, 50, 80}));
    CHECK(!msg::event_valid({EventKind::LayoutOn, 0, 0, 60, 50, 80}));     // a hotkey needs modifiers
    CHECK(!msg::event_valid({EventKind::LayoutOn, 0x06, 0x04, 60, 50, 80})); // and no regular key
    CHECK(!msg::event_valid({EventKind::LayoutOff, 0x06, 0, 60, 0, 80}));
    CHECK(!msg::event_valid({static_cast<EventKind>(0), 0, 0x04, 0, 1, 0}));
    CHECK(!msg::event_valid({static_cast<EventKind>(5), 0, 0x04, 0, 1, 0}));
    // encode/decode
    uint8_t body[1 + 6 * 33];
    size_t n = 0;
    Event ev[33];
    for (auto& e : ev) e = {EventKind::Key, 0, 0x04, 0, 10, 10};
    CHECK(msg::encode_type_body(ev, 32, body, sizeof body, &n) && n == 193);
    CHECK(!msg::encode_type_body(ev, 33, body, sizeof body, &n));
    CHECK(!msg::encode_type_body(ev, 0, body, sizeof body, &n));
    CHECK(!msg::encode_type_body(ev, 32, body, 192, &n));
    Event bad = {EventKind::Key, 0, 0x03, 0, 10, 10};
    CHECK(!msg::encode_type_body(&bad, 1, body, sizeof body, &n)); // never produces what the peer would reject
    Event dec[msg::kMaxEventsPerBatch];
    size_t cnt = 0;
    CHECK(msg::encode_type_body(ev, 32, body, sizeof body, &n) && msg::decode_type_body(body, n, dec, &cnt) == msg::EventsStatus::Ok && cnt == 32);
    CHECK(msg::decode_type_body(body, 0, dec, &cnt) == msg::EventsStatus::Truncated);
    CHECK(msg::decode_type_body(body, n - 1, dec, &cnt) == msg::EventsStatus::Truncated);
    CHECK(msg::decode_type_body(body, n + 1, dec, &cnt) == msg::EventsStatus::Truncated);
    uint8_t z[1] = {0};
    CHECK(msg::decode_type_body(z, 1, dec, &cnt) == msg::EventsStatus::Empty);
    uint8_t many[1 + 6 * 33] = {33};
    CHECK(msg::decode_type_body(many, sizeof many, dec, &cnt) == msg::EventsStatus::TooMany);
    uint8_t one[7] = {1, 0, 0, 0, 0, 0, 0};
    CHECK(msg::decode_type_body(one, 7, dec, &cnt) == msg::EventsStatus::BadEvent); // kind 0
    one[1] = 5;
    CHECK(msg::decode_type_body(one, 7, dec, &cnt) == msg::EventsStatus::BadEvent);
    // small bodies
    uint8_t s4[4];
    msg::Result res;
    CHECK(msg::encode_result({0x1234, msg::ResultCode::HidTimeout, 17}, s4) && msg::decode_result(s4, 4, &res) && res.seq == 0x1234 && res.code == msg::ResultCode::HidTimeout && res.done_events == 17);
    s4[2] = 6;
    CHECK(!msg::decode_result(s4, 4, &res));                 // unknown result code
    CHECK(!msg::decode_result(s4, 3, &res));
    msg::HelloAck ha;
    CHECK(msg::encode_hello_ack({1, 2, 3, true}, s4) && msg::decode_hello_ack(s4, 4, &ha) && ha.fw_major == 2 && ha.fw_minor == 3 && ha.usb_mounted);
    s4[3] = 2;
    CHECK(!msg::decode_hello_ack(s4, 4, &ha));               // a bool must be 0 or 1
    uint8_t s2[2];
    msg::Hello he; uint16_t sq = 0;
    CHECK(msg::encode_hello({1, 0x0F}, s2) && msg::decode_hello(s2, 2, &he) && he.caps == 0x0F && !msg::decode_hello(s2, 1, &he));
    CHECK(msg::encode_abort(0xABCD, s2) && msg::decode_abort(s2, 2, &sq) && sq == 0xABCD && !msg::decode_abort(s2, 3, &sq));
}

// ---------------------------------------------------------------- fuzzing
static msg::Event random_valid_event(Rng& r)
{
    msg::Event e;
    switch (r.below(4)) {
        case 0: e = {msg::EventKind::Key, static_cast<uint8_t>(r.below(256)), static_cast<uint8_t>(0x04 + r.below(0xE7 - 0x04 + 1)), r.byte(), static_cast<uint8_t>(1 + r.below(255)), r.byte()}; break;
        case 1: e = {msg::EventKind::Pause, 0, 0, 0, 0, r.byte()}; break;
        case 2: e = {msg::EventKind::LayoutOn, static_cast<uint8_t>(1 + r.below(255)), 0, r.byte(), static_cast<uint8_t>(1 + r.below(255)), r.byte()}; break;
        default: e = {msg::EventKind::LayoutOff, static_cast<uint8_t>(1 + r.below(255)), 0, r.byte(), static_cast<uint8_t>(1 + r.below(255)), r.byte()}; break;
    }
    return e;
}

static void mutate(Rng& r, Bytes& v)
{
    const uint32_t k = r.below(4);
    if (k == 0 && !v.empty()) v[r.below(static_cast<uint32_t>(v.size()))] ^= static_cast<uint8_t>(1u << r.below(8));
    else if (k == 1 && !v.empty()) v.erase(v.begin() + r.below(static_cast<uint32_t>(v.size())));
    else if (k == 2) v.insert(v.begin() + r.below(static_cast<uint32_t>(v.size() + 1)), r.byte());
    else if (!v.empty()) v[r.below(static_cast<uint32_t>(v.size()))] = r.byte();
}

static void fuzz_messages(Rng& r, int iters)
{
    size_t accepted = 0;
    for (int i = 0; i < iters; ++i) {
        // a valid TYPE message, then random corruption
        msg::Event ev[msg::kMaxEventsPerBatch];
        const size_t cnt = 1 + r.below(msg::kMaxEventsPerBatch);
        for (size_t k = 0; k < cnt; ++k) ev[k] = random_valid_event(r);
        uint8_t body[1 + 6 * 32], wire[300];
        size_t bn = 0, wn = 0;
        CHECK(msg::encode_type_body(ev, cnt, body, sizeof body, &bn));
        CHECK(msg::encode(msg::Type::TypeKeys, r.byte(), static_cast<uint16_t>(r.next()), body, bn, wire, sizeof wire, &wn));
        Bytes v(wire, wire + wn);
        const uint32_t m = r.below(4);                       // 0..3 mutations (0 = control: must round-trip)
        for (uint32_t k = 0; k < m; ++k) mutate(r, v);
        if (r.below(8) == 0) { v.resize(r.below(60)); for (auto& b : v) b = r.byte(); } // pure noise
        msg::Header h; const uint8_t* b = nullptr;
        if (msg::parse(v.data(), v.size(), &h, &b) == msg::ParseStatus::Ok) {
            ++accepted;
            uint8_t again[300]; size_t an = 0;
            CHECK(msg::encode(h.type, h.flags, h.seq, b, h.len, again, sizeof again, &an) && an == v.size() && std::memcmp(again, v.data(), an) == 0);
            if (h.type == msg::Type::TypeKeys) {
                msg::Event out[msg::kMaxEventsPerBatch]; size_t oc = 0;
                if (msg::decode_type_body(b, h.len, out, &oc) == msg::EventsStatus::Ok) {
                    uint8_t re[1 + 6 * 32]; size_t rn = 0;
                    CHECK(msg::encode_type_body(out, oc, re, sizeof re, &rn) && rn == h.len && std::memcmp(re, b, rn) == 0);
                    for (size_t k = 0; k < oc; ++k) CHECK(msg::event_valid(out[k]));
                }
            }
        }
        // small bodies on random bytes
        uint8_t nb[8]; const size_t nl = r.below(7);
        for (auto& x : nb) x = r.byte();
        msg::Result res; msg::Hello he; msg::HelloAck ha; uint16_t sq;
        if (msg::decode_result(nb, nl, &res)) { uint8_t o[4]; msg::encode_result(res, o); CHECK(nl == 4 && std::memcmp(o, nb, 4) == 0); }
        if (msg::decode_hello_ack(nb, nl, &ha)) { uint8_t o[4]; msg::encode_hello_ack(ha, o); CHECK(nl == 4 && std::memcmp(o, nb, 4) == 0); }
        if (msg::decode_hello(nb, nl, &he)) { uint8_t o[2]; msg::encode_hello(he, o); CHECK(nl == 2 && std::memcmp(o, nb, 2) == 0); }
        if (msg::decode_abort(nb, nl, &sq)) { uint8_t o[2]; msg::encode_abort(sq, o); CHECK(nl == 2 && std::memcmp(o, nb, 2) == 0); }
    }
    std::printf("  message fuzz: %d inputs, %zu accepted (each re-encoded identically)\n", iters, accepted);
}

// Reference stream parser: the naive, obviously-correct way.
static void ref_frames(const Bytes& s, std::vector<Bytes>* frames, bool* error)
{
    size_t p = 0;
    *error = false;
    while (p + 2 <= s.size()) {
        const size_t len = (static_cast<size_t>(s[p]) << 8) | s[p + 1];
        if (len == 0 || len > frame::kMaxPayload) { *error = true; return; }
        if (p + 2 + len > s.size()) return; // incomplete tail
        frames->push_back(Bytes(s.begin() + p + 2, s.begin() + p + 2 + len));
        p += 2 + len;
    }
}

static void fuzz_frames(Rng& r, int iters)
{
    size_t total_frames = 0, errors = 0;
    for (int i = 0; i < iters; ++i) {
        Bytes stream;
        const size_t nf = r.below(5);
        for (size_t f = 0; f < nf; ++f) {
            const size_t len = 1 + r.below(r.below(4) == 0 ? 520 : 40);
            Bytes pl(len);
            for (auto& b : pl) b = r.byte();
            const uint16_t l = static_cast<uint16_t>(len);
            stream.push_back(static_cast<uint8_t>(l >> 8)); stream.push_back(static_cast<uint8_t>(l & 0xFF));
            stream.insert(stream.end(), pl.begin(), pl.end());
        }
        const uint32_t m = r.below(3);
        for (uint32_t k = 0; k < m; ++k) mutate(r, stream);
        if (r.below(10) == 0) { stream.resize(r.below(30)); for (auto& b : stream) b = r.byte(); }
        std::vector<Bytes> want; bool want_err = false;
        ref_frames(stream, &want, &want_err);
        // the Reader, fed in random chunk sizes
        frame::Reader rd;
        std::vector<Bytes> got; bool got_err = false;
        size_t off = 0;
        while (off < stream.size() && !got_err) {
            const size_t take = std::min<size_t>(1 + r.below(r.below(3) == 0 ? 300 : 9), stream.size() - off);
            const size_t used = rd.feed(stream.data() + off, take);
            off += used;
            if (rd.state() == frame::Reader::State::FrameReady) { got.push_back(Bytes(rd.payload(), rd.payload() + rd.payload_len())); rd.next(); }
            else if (rd.state() == frame::Reader::State::Error) got_err = true;
            else if (used == 0) break; // cannot happen while NeedMore
        }
        CHECK_MSG(got == want && got_err == want_err, "iteration %d: Reader and reference disagree", i);
        total_frames += got.size();
        errors += got_err;
    }
    std::printf("  frame fuzz: %d streams, %zu frames, %zu error streams (all matched the reference parser)\n", iters, total_frames, errors);
}

// Foreign bytes into the handshake and the transport: errors, never crashes, never "success".
static void fuzz_noise(Rng& r, int iters)
{
    noise::KeyPair vs{}, ds{};
    crypto::x25519_keygen(vs.sk, vs.pk);
    crypto::x25519_keygen(ds.sk, ds.pk);
    uint8_t pro[8] = {1, 2, 3};
    size_t ok_ik = 0;
    for (int i = 0; i < iters; ++i) {
        Bytes junk(r.below(300));
        for (auto& b : junk) b = r.byte();
        uint8_t out[400];
        size_t on = 0;
        {   // IK responder given random "message 1": needs a valid ES + AEAD, so always an error
            noise::HandshakeState d;
            noise::HandshakeState::Config c;
            c.pattern = noise::Pattern::IK; c.role = noise::Role::Responder; c.prologue = pro; c.prologue_len = 8; c.s = &ds;
            CHECK(d.init(c) == noise::Status::Ok);
            const noise::Status s = d.read_message(junk.data(), junk.size(), out, sizeof out, &on);
            ok_ik += (s == noise::Status::Ok);
            CHECK(s != noise::Status::Ok && !d.complete());
        }
        {   // XX initiator given random "message 2" after a real message 1: AEAD must reject
            noise::HandshakeState v;
            noise::HandshakeState::Config c;
            c.pattern = noise::Pattern::XX; c.role = noise::Role::Initiator; c.prologue = pro; c.prologue_len = 8; c.s = &vs;
            CHECK(v.init(c) == noise::Status::Ok);
            CHECK(v.write_message(nullptr, 0, out, sizeof out, &on) == noise::Status::Ok);
            const noise::Status s = v.read_message(junk.data(), junk.size(), out, sizeof out, &on);
            CHECK(s != noise::Status::Ok && !v.complete());
        }
        {   // XX responder given random message 1 is structurally valid whenever it is >= 32 bytes: no crash is the property
            noise::HandshakeState d;
            noise::HandshakeState::Config c;
            c.pattern = noise::Pattern::XX; c.role = noise::Role::Responder; c.prologue = pro; c.prologue_len = 8; c.s = &ds;
            CHECK(d.init(c) == noise::Status::Ok);
            const noise::Status s = d.read_message(junk.data(), junk.size(), out, sizeof out, &on);
            CHECK((junk.size() >= 32) == (s == noise::Status::Ok));
        }
        {   // transport with a random key: random ciphertext is never accepted
            noise::CipherState cs;
            uint8_t k[32]; crypto::random_bytes(k, 32);
            cs.init_key(k);
            CHECK(cs.decrypt(nullptr, 0, junk.data(), junk.size(), out, sizeof out, &on) != noise::Status::Ok);
        }
    }
    CHECK(ok_ik == 0);
    std::printf("  noise fuzz: %d rounds x 4 targets, no crash, nothing accepted that should not be\n", iters);
}

int main()
{
    test_frames();
    test_messages();
    test_events();
    Rng r{0xC0FFEE1234ABCDull};
    fuzz_messages(r, 150000);
    fuzz_frames(r, 60000);
    fuzz_noise(r, 3000);
    return finish("codec");
}
