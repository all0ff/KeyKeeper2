// Host tests for the UART link test logic (framing, ping payload, peer accounting, verdict).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "../main/link_frame.hpp"
#include "../main/link_stats.hpp"

using namespace uartlink;

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) {                                                                 \
            ++g_failed;                                                                \
            if (g_failed <= 20) std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                              \
    } while (0)

using Bytes = std::vector<uint8_t>;

static Bytes make_payload(std::mt19937& rng, size_t len)
{
    Bytes p(len);
    for (auto& b : p) {
        b = static_cast<uint8_t>(rng());
    }
    return p;
}

static Bytes frame_of(const Bytes& payload)
{
    Bytes out(MAX_FRAME);
    const size_t n = encode(payload.data(), payload.size(), out.data(), out.size());
    out.resize(n);
    return out;
}

static void test_crc()
{
    const char* s = "123456789";
    CHECK(crc16(reinterpret_cast<const uint8_t*>(s), 9) == 0x29B1);  // CRC-16/CCITT-FALSE check value
    CHECK(crc16(nullptr, 0) == 0xFFFF);
}

static void test_encode_limits()
{
    uint8_t out[MAX_FRAME];
    uint8_t pl[MAX_PAYLOAD + 1] = {};
    CHECK(encode(pl, MAX_PAYLOAD + 1, out, sizeof out) == 0);
    CHECK(encode(pl, 10, out, 14) == 0);       // needs 15
    CHECK(encode(pl, 10, out, 15) == 15);
    CHECK(encode(pl, 0, out, 5) == 5);
    CHECK(encode(pl, MAX_PAYLOAD, out, sizeof out) == MAX_FRAME);
}

static void test_roundtrip_all_lengths()
{
    std::mt19937 rng(1);
    for (size_t len = 0; len <= MAX_PAYLOAD; ++len) {
        const Bytes pl = make_payload(rng, len);
        const Bytes fr = frame_of(pl);
        CHECK(fr.size() == HEADER + len + TRAILER);

        Parser parser;
        std::vector<Bytes> got;
        parser.feed(fr.data(), fr.size(), [&](const uint8_t* p, size_t n) { got.emplace_back(p, p + n); });
        CHECK(got.size() == 1);
        CHECK(got.size() == 1 && got[0] == pl);
        CHECK(parser.frames_ok == 1 && parser.bad_crc == 0 && parser.junk_bytes == 0);
        CHECK(parser.buffered() == 0);
    }
}

// Payload that itself contains the sync pair and a "length" that looks plausible.
static void test_payload_with_sync_inside()
{
    Bytes pl = {0xA5, 0x5A, 0x05, 0xA5, 0x5A, 0x00, 0xA5, 0x5A, 0xFF, 0x12, 0x34};
    const Bytes fr = frame_of(pl);
    Parser parser;
    std::vector<Bytes> got;
    for (int i = 0; i < 3; ++i) {
        parser.feed(fr.data(), fr.size(), [&](const uint8_t* p, size_t n) { got.emplace_back(p, p + n); });
    }
    CHECK(got.size() == 3);
    for (const auto& g : got) {
        CHECK(g == pl);
    }
    CHECK(parser.bad_crc == 0);
}

// Every single-bit error anywhere in a frame must never produce a corrupted frame, and the good
// frames that follow must still be delivered (that is what resync is for).
static void test_single_bit_errors_and_resync()
{
    std::mt19937 rng(2);
    const size_t lens[] = {0, 1, 10, 24, 60, MAX_PAYLOAD};
    for (size_t len : lens) {
        const Bytes pl = make_payload(rng, len);
        const Bytes fr = frame_of(pl);
        // The good frames that follow: distinct payloads so we can tell them apart.
        std::vector<Bytes> tail_payloads;
        Bytes tail;
        for (int i = 0; i < 16; ++i) {
            Bytes t = make_payload(rng, 24);
            t[0] = static_cast<uint8_t>(i);
            tail_payloads.push_back(t);
            const Bytes tf = frame_of(t);
            tail.insert(tail.end(), tf.begin(), tf.end());
        }

        for (size_t bit = 0; bit < fr.size() * 8; ++bit) {
            Bytes bad = fr;
            bad[bit / 8] = static_cast<uint8_t>(bad[bit / 8] ^ (1u << (bit % 8)));

            Parser parser;
            std::vector<Bytes> got;
            auto cb = [&](const uint8_t* p, size_t n) { got.emplace_back(p, p + n); };
            parser.feed(bad.data(), bad.size(), cb);
            parser.feed(tail.data(), tail.size(), cb);

            // Nothing corrupted was accepted: every delivered frame is one of the tail frames, in order.
            size_t next_tail = 0;
            bool all_known = true;
            for (const auto& g : got) {
                while (next_tail < tail_payloads.size() && tail_payloads[next_tail] != g) {
                    ++next_tail;
                }
                if (next_tail == tail_payloads.size()) {
                    all_known = false;
                    break;
                }
                ++next_tail;
            }
            CHECK(all_known);
            // And none of the good tail frames was lost.
            CHECK(got.size() == tail_payloads.size());
            CHECK(parser.buffered() == 0);
        }
    }
}

// Random garbage (including false sync pairs) before and between frames.
static void test_garbage_resync()
{
    std::mt19937 rng(3);
    for (int trial = 0; trial < 3000; ++trial) {
        Bytes stream;
        std::vector<Bytes> expect;
        const int nframes = 6;
        for (int i = 0; i < nframes; ++i) {
            const size_t glen = rng() % 40;
            for (size_t k = 0; k < glen; ++k) {
                // Bias towards sync bytes to provoke false candidates.
                const uint32_t r = rng() % 4;
                stream.push_back(r == 0 ? SYNC0 : r == 1 ? SYNC1 : static_cast<uint8_t>(rng()));
            }
            Bytes pl = make_payload(rng, 10 + rng() % 30);
            pl[0] = static_cast<uint8_t>(i);
            expect.push_back(pl);
            const Bytes fr = frame_of(pl);
            stream.insert(stream.end(), fr.begin(), fr.end());
        }
        // Flush: enough clean frames that any unfinished false candidate (up to MAX_FRAME bytes) completes.
        for (int i = 0; i < 8; ++i) {
            const Bytes fr = frame_of(Bytes(40, 0x77));
            stream.insert(stream.end(), fr.begin(), fr.end());
        }

        Parser parser;
        std::vector<Bytes> got;
        parser.feed(stream.data(), stream.size(), [&](const uint8_t* p, size_t n) { got.emplace_back(p, p + n); });

        // Our real frames must appear, in order, as a subsequence of what was delivered (a false
        // candidate could in theory deliver an extra frame, but only with a valid CRC: 2^-16).
        size_t e = 0;
        for (const auto& g : got) {
            if (e < expect.size() && g == expect[e]) {
                ++e;
            }
        }
        CHECK(e == expect.size());
        CHECK(parser.buffered() == 0);
    }
}

static void test_fuzz_no_crash()
{
    std::mt19937 rng(4);
    Parser parser;
    uint64_t delivered = 0;
    for (int i = 0; i < 2000000; ++i) {
        const uint32_t r = rng() % 8;
        const uint8_t b = r == 0 ? SYNC0 : r == 1 ? SYNC1 : r == 2 ? static_cast<uint8_t>(rng() % 130)
                                                                   : static_cast<uint8_t>(rng());
        parser.feed(b, [&](const uint8_t*, size_t n) {
            ++delivered;
            CHECK(n <= MAX_PAYLOAD);
        });
        if (parser.buffered() >= MAX_FRAME) {
            CHECK(false);
            return;
        }
    }
    CHECK(parser.frames_ok == delivered);
}

static void test_ping_codec()
{
    for (size_t len = PING_MIN; len <= MAX_PAYLOAD; ++len) {
        Ping p;
        p.id = 0xBEEF;
        p.seq = 0x01020304u + static_cast<uint32_t>(len);
        p.echo = 0xCAFEBABEu;
        Bytes buf(len);
        CHECK(encode_ping(p, len, buf.data()) == len);
        Ping q;
        CHECK(decode_ping(buf.data(), len, q) == PingResult::Ok);
        CHECK(q.id == p.id && q.seq == p.seq && q.echo == p.echo);
        if (len > PING_MIN) {
            Bytes bad = buf;
            bad[len - 1] = static_cast<uint8_t>(bad[len - 1] ^ 1u);
            CHECK(decode_ping(bad.data(), len, q) == PingResult::BadFill);
        }
    }
    Ping p;
    uint8_t buf[16] = {};
    CHECK(encode_ping(p, PING_MIN - 1, buf) == 0);
    CHECK(encode_ping(p, MAX_PAYLOAD + 1, buf) == 0);
    Ping q;
    CHECK(decode_ping(buf, PING_MIN - 1, q) == PingResult::TooShort);
}

static void test_peer_tracker()
{
    PeerTracker t;
    Ping p;
    p.id = 0x1111;

    p.seq = 7;  // first frame seen: no loss, even though seq is not 0
    t.on_ping(p);
    CHECK(t.ok == 1 && t.lost == 0 && t.restarts == 0);

    p.seq = 8;
    t.on_ping(p);
    CHECK(t.lost == 0);

    p.seq = 12;  // 9, 10, 11 missing
    t.on_ping(p);
    CHECK(t.lost == 3 && t.restarts == 0);

    p.seq = 0;  // peer rebooted
    t.on_ping(p);
    CHECK(t.lost == 3 && t.restarts == 1);

    p.seq = 1;
    t.on_ping(p);
    CHECK(t.lost == 3 && t.restarts == 1);

    p.id = 0x2222;  // a different board
    p.seq = 500;
    t.on_ping(p);
    CHECK(t.restarts == 2 && t.lost == 3);
    CHECK(t.ok == 6);
}

static void test_sees_me()
{
    PeerTracker t;
    CHECK(!t.sees_me(100));  // nothing heard yet

    Ping p;
    p.id = 1;
    p.seq = 0;
    p.echo = NO_ECHO;
    t.on_ping(p);
    CHECK(!t.sees_me(100));  // peer has not heard me yet

    p.seq = 1;
    p.echo = 90;  // peer heard my frame 90
    t.on_ping(p);
    CHECK(t.sees_me(100));           // last sent is 99: 9 behind, fine
    CHECK(t.sees_me(115));           // 24 behind
    CHECK(t.sees_me(116));           // 25 behind, still within the window
    CHECK(!t.sees_me(117));          // 26 behind: stale
    CHECK(!t.sees_me(50));           // my counter went back (I rebooted): echo is from the old run
    CHECK(!t.sees_me(0));
}

static void test_verdict()
{
    CHECK(classify(1, 0, 0, false) == Verdict::Loopback);
    CHECK(classify(5, 500, 5, true) == Verdict::Loopback);
    CHECK(classify(0, 0, 0, false) == Verdict::NoData);
    CHECK(classify(0, 0, 0, true) == Verdict::NoData);
    CHECK(classify(0, 40, 0, false) == Verdict::Garbage);
    CHECK(classify(0, 40, 0, true) == Verdict::Garbage);
    CHECK(classify(0, 40, 3, false) == Verdict::OneWay);
    CHECK(classify(0, 40, 3, true) == Verdict::Ok);
    for (Verdict v : {Verdict::Loopback, Verdict::NoData, Verdict::Garbage, Verdict::OneWay, Verdict::Ok}) {
        CHECK(std::strlen(verdict_tag(v)) > 0);
        CHECK(std::strlen(verdict_hint(v)) > 0);
    }
}

// Two endpoints over a noisy channel, the same way main.cpp uses the pieces. Invariants:
//  * nothing corrupted is accepted (decode_ping would report BadFill, or ids/seq would be wrong);
//  * received + lost == span of sequence numbers seen (the accounting is exact when there are no restarts).
struct Endpoint {
    Endpoint(uint16_t id_, size_t payload_len_) : id(id_), payload_len(payload_len_) {}

    uint16_t id;
    size_t payload_len;
    uint32_t tx_seq = 0;
    Parser parser;
    PeerTracker peer;
    uint32_t self_frames = 0;
    uint32_t bad_payload = 0;
    uint32_t first_seq = 0;
    bool have_first = false;

    Bytes next_frame()
    {
        Ping p;
        p.id = id;
        p.seq = tx_seq++;
        p.echo = peer.have ? peer.last_seq : NO_ECHO;
        Bytes pl(payload_len);
        encode_ping(p, payload_len, pl.data());
        return frame_of(pl);
    }

    void receive(const Bytes& bytes)
    {
        parser.feed(bytes.data(), bytes.size(), [&](const uint8_t* d, size_t n) {
            Ping q;
            if (decode_ping(d, n, q) != PingResult::Ok) {
                ++bad_payload;
                return;
            }
            if (q.id == id) {
                ++self_frames;
                return;
            }
            if (!have_first) {
                have_first = true;
                first_seq = q.seq;
            }
            peer.on_ping(q);
        });
    }
};

static void test_two_endpoints_noisy_channel()
{
    std::mt19937 rng(5);
    for (int scenario = 0; scenario < 6; ++scenario) {
        const double ber = scenario == 0 ? 0.0 : scenario == 1 ? 1e-5 : scenario == 2 ? 1e-4 : scenario == 3 ? 1e-3 : 3e-3;
        Endpoint a{0x00A1, scenario == 5 ? MAX_PAYLOAD : 24};
        Endpoint b{0x00B2, scenario == 5 ? MAX_PAYLOAD : 24};
        std::uniform_real_distribution<double> u(0.0, 1.0);

        auto corrupt = [&](Bytes bytes) {
            for (auto& byte : bytes) {
                for (int bit = 0; bit < 8; ++bit) {
                    if (u(rng) < ber) {
                        byte = static_cast<uint8_t>(byte ^ (1u << bit));
                    }
                }
            }
            return bytes;
        };

        const int rounds = 20000;
        for (int i = 0; i < rounds; ++i) {
            b.receive(corrupt(a.next_frame()));
            a.receive(corrupt(b.next_frame()));
        }
        // Two clean frames each way so a candidate left half-parsed by noise is finished.
        for (int i = 0; i < 6; ++i) {
            b.receive(a.next_frame());
            a.receive(b.next_frame());
        }

        for (const Endpoint* e : {&a, &b}) {
            // A frame that passes CRC16 never carries a wrong payload on a sane wire. At absurd noise
            // (BER >= 1e-3, almost every frame corrupted) about 1 corrupted frame in 83000 still passes
            // CRC16 (measured; theory says ~2^-16). The payload check catches those: they are counted
            // in bad_payload and must stay rare.
            if (ber < 1e-3) {
                CHECK(e->bad_payload == 0);
            } else {
                CHECK(e->bad_payload <= 6);
            }
            CHECK(e->self_frames == 0);
            CHECK(e->peer.restarts == 0);
            CHECK(e->peer.have);
            // received + lost == span of sequence numbers observed
            const uint64_t span = static_cast<uint64_t>(e->peer.last_seq) - e->first_seq + 1u;
            CHECK(e->peer.ok + e->peer.lost == span);
            if (ber == 0.0) {
                CHECK(e->peer.lost == 0);
                CHECK(e->peer.ok == static_cast<uint32_t>(rounds + 6));
            } else {
                CHECK(e->peer.ok > 0);
            }
            CHECK(e->parser.buffered() == 0);
        }
        // Each side can tell the other one hears it.
        CHECK(a.peer.sees_me(a.tx_seq));
        CHECK(b.peer.sees_me(b.tx_seq));
    }
}

// A board with TX wired to RX hears itself and must say so.
static void test_loopback()
{
    Endpoint a{0x00A1, 24};
    for (int i = 0; i < 50; ++i) {
        a.receive(a.next_frame());
    }
    CHECK(a.self_frames == 50);
    CHECK(!a.peer.have);
    CHECK(classify(a.self_frames, 50 * 29, 0, false) == Verdict::Loopback);
}

// Real life: the second board is switched on in the middle of a frame.
static void test_join_mid_frame()
{
    Endpoint a{0x00A1, 24};
    Endpoint b{0x00B2, 24};
    for (int i = 0; i < 5; ++i) {
        a.next_frame();  // a has been sending for a while
    }
    Bytes f = a.next_frame();
    Bytes partial(f.begin() + 11, f.end());  // b only sees the tail of one frame
    b.receive(partial);
    for (int i = 0; i < 10; ++i) {
        b.receive(a.next_frame());
    }
    CHECK(b.peer.ok == 10);
    CHECK(b.peer.lost == 0);
    CHECK(b.bad_payload == 0);
}

int main()
{
    test_crc();
    test_encode_limits();
    test_roundtrip_all_lengths();
    test_payload_with_sync_inside();
    test_single_bit_errors_and_resync();
    test_garbage_resync();
    test_fuzz_no_crash();
    test_ping_codec();
    test_peer_tracker();
    test_sees_me();
    test_verdict();
    test_two_endpoints_noisy_channel();
    test_loopback();
    test_join_mid_frame();

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
