// Host tests for the pairing flow: two (or four) simulated boards joined by simulated UART wires that
// carry real uartlink frames, with real kkproto crypto underneath.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include "../../uart_link_test/main/link_frame.hpp"
#include "../main/pairing_flow.hpp"
#include "kkproto/crypto_port.hpp"

using namespace pairtest;

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) {                                                                 \
            ++g_failed;                                                                \
            if (g_failed <= 25) std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                              \
    } while (0)

using Bytes = std::vector<uint8_t>;
using ChannelFn = std::function<bool(Bytes&)>;  // may corrupt the frame; returns false to drop it

struct Wire {
    std::deque<uint8_t> q;
    ChannelFn fn;
    std::vector<Bytes> seen;  // every frame that was put on the wire (for replay tests)
};

struct Node : Io {
    explicit Node(const Peer::Params& prm)
    {
        CHECK(kk::crypto::x25519_keygen(key.sk, key.pk));
        peer = std::make_unique<Peer>(*this, key, prm);
    }

    kk::noise::KeyPair key{};
    std::unique_ptr<Peer> peer;
    uartlink::Parser parser;
    Wire* tx = nullptr;
    Wire* rx = nullptr;
    std::vector<Event> events;
    bool powered = true;
    bool hold_while_off = false;  // keep the bytes queued while unpowered (a pipe), instead of losing them

    void send(const uint8_t* p, size_t n) override
    {
        Bytes fr(uartlink::MAX_FRAME);
        const size_t m = uartlink::encode(p, n, fr.data(), fr.size());
        CHECK(m != 0);
        fr.resize(m);
        if (tx == nullptr) {
            return;
        }
        if (tx->fn && !tx->fn(fr)) {
            return;
        }
        tx->seen.push_back(fr);
        for (uint8_t b : fr) {
            tx->q.push_back(b);
        }
    }
    void event(Event e) override { events.push_back(e); }

    void pump(uint32_t now)
    {
        if (!powered && hold_while_off) {
            return;
        }
        while (rx != nullptr && !rx->q.empty()) {
            const uint8_t b = rx->q.front();
            rx->q.pop_front();
            if (powered) {
                parser.feed(b, [&](const uint8_t* d, size_t n) { peer->on_payload(d, n, now); });
            }
        }
    }
    void tick(uint32_t now)
    {
        if (powered) {
            peer->tick(now);
        }
    }
    void power_on()
    {
        powered = true;
        parser = uartlink::Parser();
        peer->reset();
    }
    bool saw(Event e) const
    {
        for (Event x : events) {
            if (x == e) return true;
        }
        return false;
    }
};

static void run(std::vector<Node*> nodes, uint32_t from_ms, uint32_t to_ms)
{
    for (uint32_t t = from_ms; t < to_ms; ++t) {
        for (Node* n : nodes) n->pump(t);
        for (Node* n : nodes) n->tick(t);
    }
}

static bool six_digits(const char* s)
{
    if (s == nullptr || std::strlen(s) != 6) return false;
    for (int i = 0; i < 6; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}

static bool same_key(const uint8_t* a, const uint8_t* b) { return a != nullptr && b != nullptr && std::memcmp(a, b, 32) == 0; }

static bool both_paired(const Node& a, const Node& b)
{
    return a.peer->phase() == Phase::Paired && b.peer->phase() == Phase::Paired;
}

static bool order_is(const std::vector<Event>& got, std::initializer_list<Event> want)
{
    size_t i = 0;
    for (Event e : got) {
        if (i < want.size() && e == *(want.begin() + i)) ++i;
    }
    return i == want.size();
}

// --------------------------------------------------------------------------------------------------

static void test_normal_pairing()
{
    Peer::Params prm;
    Node a(prm), b(prm);
    Wire ab, ba;
    a.tx = &ab; b.rx = &ab;
    b.tx = &ba; a.rx = &ba;

    a.peer->start_as_initiator(0);
    run({&a, &b}, 0, 500);

    CHECK(both_paired(a, b));
    CHECK(six_digits(a.peer->code()) && six_digits(b.peer->code()));
    CHECK(a.peer->code() != nullptr && b.peer->code() != nullptr && std::strcmp(a.peer->code(), b.peer->code()) == 0);
    CHECK(same_key(a.peer->peer_static(), b.key.pk));
    CHECK(same_key(b.peer->peer_static(), a.key.pk));
    CHECK(a.peer->attempts() == 1);
    CHECK(a.peer->is_initiator() && !b.peer->is_initiator());
    CHECK(order_is(a.events, {Event::SentMsg1, Event::GotMsg2, Event::SentMsg3, Event::SasReady, Event::SentHello, Event::SessionOk}));
    CHECK(order_is(b.events, {Event::GotMsg1, Event::SentMsg2, Event::GotMsg3, Event::SasReady, Event::GotHello,
                              Event::SentHelloAck, Event::SessionOk}));
    CHECK(ab.seen.size() == 3 && ba.seen.size() == 2);  // msg1, msg3, HELLO / msg2, HELLO_ACK
}

static void test_repair_gives_new_code()
{
    Peer::Params prm;
    Node a(prm), b(prm);
    Wire ab, ba;
    a.tx = &ab; b.rx = &ab;
    b.tx = &ba; a.rx = &ba;

    a.peer->start_as_initiator(0);
    run({&a, &b}, 0, 500);
    CHECK(both_paired(a, b));
    const std::string first = a.peer->code();

    a.peer->start_as_initiator(1000);  // user presses the button again
    run({&a, &b}, 1000, 1500);
    CHECK(both_paired(a, b));
    CHECK(std::strcmp(a.peer->code(), b.peer->code()) == 0);
    CHECK(first != a.peer->code());  // fresh ephemeral keys: new code (equal with probability 1e-6)
    CHECK(same_key(a.peer->peer_static(), b.key.pk));

    // The responder can also be re-paired from the OTHER side.
    b.peer->start_as_initiator(2000);
    run({&a, &b}, 2000, 2500);
    CHECK(both_paired(a, b));
    CHECK(std::strcmp(a.peer->code(), b.peer->code()) == 0);
    CHECK(b.peer->is_initiator() && !a.peer->is_initiator());
}

static void test_responder_boots_late()
{
    Peer::Params prm;
    Node a(prm), b(prm);
    Wire ab, ba;
    a.tx = &ab; b.rx = &ab;
    b.tx = &ba; a.rx = &ba;

    b.powered = false;
    a.peer->start_as_initiator(0);
    run({&a, &b}, 0, 2500);
    CHECK(a.peer->phase() == Phase::InitWaitReply);
    CHECK(a.peer->attempts() == 3);  // initial + retries at 1000 and 2000

    b.power_on();
    run({&a, &b}, 2500, 6000);
    CHECK(both_paired(a, b));
    CHECK(std::strcmp(a.peer->code(), b.peer->code()) == 0);
    CHECK(a.peer->attempts() >= 3 && a.peer->attempts() <= 4);
}

// The wire keeps earlier msg1 frames while the other board boots, and the board then answers ALL of them.
// The initiator sees replies to attempts that no longer exist; it must not give up because of them.
static void test_stale_replies_do_not_kill_the_initiator()
{
    Peer::Params prm;
    Node a(prm), b(prm);
    Wire ab, ba;
    a.tx = &ab; b.rx = &ab;
    b.tx = &ba; a.rx = &ba;

    b.powered = false;
    b.hold_while_off = true;
    a.peer->start_as_initiator(0);
    run({&a, &b}, 0, 2500);
    CHECK(ab.seen.size() == 3);  // three msg1 frames are waiting for B

    b.power_on();
    run({&a, &b}, 2500, 8000);
    CHECK(a.peer->phase() != Phase::Failed);
    CHECK(both_paired(a, b));
    CHECK(std::strcmp(a.peer->code(), b.peer->code()) == 0);
    CHECK(a.peer->attempts() == 4);  // the first attempt after B was ready (at 3000 ms) succeeded
}

static void test_nobody_answers()
{
    Peer::Params prm;
    Node a(prm), b(prm);
    Wire ab, ba;
    a.tx = &ab; b.rx = &ab;
    b.tx = &ba; a.rx = &ba;

    b.powered = false;
    a.peer->start_as_initiator(0);
    run({&a, &b}, 0, 20000);
    CHECK(a.peer->phase() == Phase::Failed);
    CHECK(a.peer->fail_reason() == Fail::NoReply);
    CHECK(a.peer->attempts() == prm.max_attempts);
    CHECK(a.peer->code() == nullptr);
    CHECK(ab.seen.size() == prm.max_attempts);  // it did not keep spamming after giving up
}

static void test_both_start_at_once()
{
    int paired = 0;
    for (int trial = 0; trial < 60; ++trial) {
        Peer::Params prm;
        Node a(prm), b(prm);
        Wire ab, ba;
        a.tx = &ab; b.rx = &ab;
        b.tx = &ba; a.rx = &ba;

        a.peer->start_as_initiator(0);
        b.peer->start_as_initiator(0);
        run({&a, &b}, 0, 1500);
        if (both_paired(a, b)) {
            ++paired;
            CHECK(std::strcmp(a.peer->code(), b.peer->code()) == 0);
            CHECK(a.peer->is_initiator() != b.peer->is_initiator());  // exactly one of them yielded
            CHECK(a.saw(Event::Yield) != b.saw(Event::Yield));
        }
    }
    CHECK(paired == 60);
}

static void test_dying_initiator_is_forgotten()
{
    Peer::Params prm;
    Node a(prm), b(prm);
    Wire ab, ba;
    a.tx = &ab; b.rx = &ab;
    b.tx = &ba; a.rx = &ba;

    a.peer->start_as_initiator(0);
    run({&a, &b}, 0, 1);  // B has received msg1 and answered; A has not yet seen msg2
    CHECK(b.peer->phase() == Phase::RespWaitFinal);
    a.powered = false;  // cable pulled
    run({&a, &b}, 1, 6000);
    CHECK(b.peer->phase() == Phase::Idle);
    CHECK(b.saw(Event::Dropped));
    CHECK(b.peer->code() == nullptr);
}

// Lost messages at any point: the initiator restarts and pairing still completes. The wire noise and
// the number of attempts are parameters; the expected success rate follows from the frame sizes
// (msg1 37 B, msg2 101 B, msg3 69 B, HELLO and HELLO_ACK about 30 B: one attempt needs all five intact).
static void noisy_wire_trials(double ber, uint8_t max_attempts, uint32_t run_ms, int trials, int min_paired_percent,
                              unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    int paired = 0, retried = 0;
    for (int trial = 0; trial < trials; ++trial) {
        Peer::Params prm;
        prm.max_attempts = max_attempts;
        Node a(prm), b(prm);
        Wire ab, ba;
        a.tx = &ab; b.rx = &ab;
        b.tx = &ba; a.rx = &ba;
        auto noise = [&](Bytes& fr) {
            for (auto& byte : fr) {
                for (int bit = 0; bit < 8; ++bit) {
                    if (u(rng) < ber) byte = static_cast<uint8_t>(byte ^ (1u << bit));
                }
            }
            return true;
        };
        ab.fn = noise;
        ba.fn = noise;

        a.peer->start_as_initiator(0);
        run({&a, &b}, 0, run_ms);
        if (both_paired(a, b)) {
            ++paired;
            CHECK(std::strcmp(a.peer->code(), b.peer->code()) == 0);
            CHECK(same_key(a.peer->peer_static(), b.key.pk));
            CHECK(same_key(b.peer->peer_static(), a.key.pk));
            if (a.peer->attempts() > 1) ++retried;
        } else {
            // If it did not make it, it must say so, never claim a half-done success.
            CHECK(a.peer->phase() == Phase::Failed || a.peer->phase() == Phase::Paired);
        }
    }
    CHECK(paired * 100 >= trials * min_paired_percent);
    CHECK(retried > 0);  // the noise really did force restarts
}

static void test_noisy_wire_pairs_anyway()
{
    noisy_wire_trials(1e-4, 8, 20000, 300, 99, 11);    // a bad wire: ~80% per attempt, 8 attempts
    noisy_wire_trials(1e-3, 40, 45000, 100, 95, 21);   // an awful wire: ~12% per attempt, 40 attempts
}

// Heavy corruption: whatever the outcome, "both paired" must always mean matching codes and keys.
static void test_heavy_noise_never_mismatches()
{
    std::mt19937 rng(12);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    int agreed = 0;
    for (int trial = 0; trial < 300; ++trial) {
        Peer::Params prm;
        prm.max_attempts = 30;
        Node a(prm), b(prm);
        Wire ab, ba;
        a.tx = &ab; b.rx = &ab;
        b.tx = &ba; a.rx = &ba;
        auto noise = [&](Bytes& fr) {
            for (auto& byte : fr) {
                for (int bit = 0; bit < 8; ++bit) {
                    if (u(rng) < 4e-3) byte = static_cast<uint8_t>(byte ^ (1u << bit));
                }
            }
            return true;
        };
        ab.fn = noise;
        ba.fn = noise;
        a.peer->start_as_initiator(0);
        run({&a, &b}, 0, 40000);
        if (both_paired(a, b)) {
            ++agreed;
            CHECK(std::strcmp(a.peer->code(), b.peer->code()) == 0);
            CHECK(same_key(a.peer->peer_static(), b.key.pk));
        }
    }
    CHECK(agreed > 0);
}

// A man in the middle can relay everything and even complete both handshakes, but the two ends then
// show DIFFERENT codes: that is the whole point of comparing them.
static void test_man_in_the_middle_shows_different_codes()
{
    int equal = 0;
    const int trials = 60;
    for (int trial = 0; trial < trials; ++trial) {
        Peer::Params prm;
        Node a(prm), m1(prm), m2(prm), b(prm);  // A <-> M1 ... M2 <-> B
        Wire a_m1, m1_a, m2_b, b_m2;
        a.tx = &a_m1; m1.rx = &a_m1;
        m1.tx = &m1_a; a.rx = &m1_a;
        m2.tx = &m2_b; b.rx = &m2_b;
        b.tx = &b_m2; m2.rx = &b_m2;

        a.peer->start_as_initiator(0);
        m2.peer->start_as_initiator(0);
        run({&a, &m1, &m2, &b}, 0, 1500);

        CHECK(both_paired(a, m1));
        CHECK(both_paired(m2, b));
        CHECK(std::strcmp(a.peer->code(), m1.peer->code()) == 0);
        CHECK(std::strcmp(m2.peer->code(), b.peer->code()) == 0);
        CHECK(same_key(a.peer->peer_static(), m1.key.pk));  // A believes it talks to M's key, not B's
        CHECK(!same_key(a.peer->peer_static(), b.key.pk));
        if (std::strcmp(a.peer->code(), b.peer->code()) == 0) ++equal;
    }
    CHECK(equal == 0);  // chance of a coincidence per attempt: 1e-6
}

// Replaying a captured encrypted message must kill the session, not be accepted again.
static void test_replayed_hello_is_rejected()
{
    Peer::Params prm;
    Node a(prm), b(prm);
    Wire ab, ba;
    a.tx = &ab; b.rx = &ab;
    b.tx = &ba; a.rx = &ba;
    a.peer->start_as_initiator(0);
    run({&a, &b}, 0, 500);
    CHECK(both_paired(a, b));

    CHECK(ab.seen.size() == 3);
    const Bytes hello = ab.seen.back();
    for (uint8_t byte : hello) ab.q.push_back(byte);
    run({&a, &b}, 500, 600);
    CHECK(b.peer->phase() == Phase::Failed);
    CHECK(b.peer->fail_reason() == Fail::Transport);
}

// A stray frame of the wrong size while a pairing is in progress is ignored; it neither aborts nor
// derails the pairing.
static void test_stray_frames_do_not_derail_pairing()
{
    Peer::Params prm;
    Node a(prm), b(prm);
    Wire ab, ba;
    a.tx = &ab; b.rx = &ab;
    b.tx = &ba; a.rx = &ba;

    a.peer->start_as_initiator(0);
    run({&a, &b}, 0, 1);  // B answered msg1; A has not yet seen msg2
    CHECK(a.peer->phase() == Phase::InitWaitReply);
    CHECK(b.peer->phase() == Phase::RespWaitFinal);

    uint8_t junk[64];
    for (size_t i = 0; i < sizeof junk; ++i) junk[i] = static_cast<uint8_t>(i * 7 + 1);
    a.peer->on_payload(junk, 50, 0);  // neither 32 nor 96 bytes
    a.peer->on_payload(junk, 0, 0);
    b.peer->on_payload(junk, 10, 0);  // neither 32 nor 64 bytes
    CHECK(a.peer->phase() == Phase::InitWaitReply);
    CHECK(b.peer->phase() == Phase::RespWaitFinal);

    run({&a, &b}, 1, 500);
    CHECK(both_paired(a, b));
    CHECK(std::strcmp(a.peer->code(), b.peer->code()) == 0);
}

// Garbage must never crash or wedge a board (run under ASan/UBSan where available).
static void test_garbage_payloads()
{
    std::mt19937 rng(13);
    Peer::Params prm;
    Node b(prm);
    Wire sink;
    b.tx = &sink;
    uint32_t now = 0;
    for (int i = 0; i < 20000; ++i) {
        const uint32_t kind = rng() % 6;
        size_t n = kind == 0 ? kMsg1Len : kind == 1 ? kMsg2Len : kind == 2 ? kMsg3Len : rng() % 256;
        Bytes p(n);
        for (auto& x : p) x = static_cast<uint8_t>(rng());
        now += 1 + rng() % 50;
        b.peer->on_payload(p.data(), p.size(), now);
        b.peer->tick(now);
        sink.q.clear();
    }
    // After a quiet spell a board that is not paired goes back to a sane resting state.
    now += 10000;
    b.peer->tick(now);
    const Phase ph = b.peer->phase();
    CHECK(ph == Phase::Idle || ph == Phase::Failed || ph == Phase::Paired);

    // Too-short and oversized inputs.
    uint8_t big[1024] = {};
    b.peer->on_payload(big, 0, now);
    b.peer->on_payload(big, 1, now);
    b.peer->on_payload(big, sizeof big, now);
    CHECK(true);
}

int main()
{
    CHECK(kk::crypto::selftest() == nullptr);
    test_normal_pairing();
    test_repair_gives_new_code();
    test_responder_boots_late();
    test_stale_replies_do_not_kill_the_initiator();
    test_nobody_answers();
    test_both_start_at_once();
    test_dying_initiator_is_forgotten();
    test_noisy_wire_pairs_anyway();
    test_heavy_noise_never_mismatches();
    test_man_in_the_middle_shows_different_codes();
    test_replayed_hello_is_rejected();
    test_stray_frames_do_not_derail_pairing();
    test_garbage_payloads();

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
