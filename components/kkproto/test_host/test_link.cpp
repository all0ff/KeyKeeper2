// Host tests for kk::link::Endpoint: a vault and a dongle talking over a simulated wire, with a
// simulated clock, frame loss, corruption, reboots, strangers and a man in the middle.
#include "kkproto/crypto_port.hpp"
#include "kkproto/link.hpp"
#include "test_support.hpp"

#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

using namespace kk;
using link::Event;
using link::Role;
using link::State;

namespace {

struct Wire {
    std::deque<Bytes> q;
    // Return false to drop the frame. May modify it.
    std::function<bool(Bytes&)> filter;
    int sent = 0, dropped = 0;
    std::vector<Bytes> seen;
    void put(const uint8_t* d, size_t n)
    {
        Bytes b(d, d + n);
        ++sent;
        seen.push_back(b);
        if (filter && !filter(b)) {
            ++dropped;
            return;
        }
        q.push_back(std::move(b));
    }
};

struct Msg {
    msg::Type type;
    Bytes body;
};

struct Node : link::Io {
    noise::KeyPair key{};
    std::unique_ptr<link::Endpoint> ep;
    Wire* out = nullptr;
    Wire* in = nullptr;
    bool powered = true;
    bool hold_while_off = false;
    link::Params prm;
    Role role;
    std::vector<Event> events;
    std::vector<Msg> msgs;
    Bytes done_peer;  ///< peer key reported with PairingDone
    char code_at_ready[7] = {};

    Node(Role r, const link::Params& p) : prm(p), role(r)
    {
        crypto::x25519_keygen(key.sk, key.pk);
        boot();
    }
    void boot()
    {
        ep = std::make_unique<link::Endpoint>(role, *this, key, prm);
    }
    // link::Io
    void send(const uint8_t* d, size_t n) override
    {
        if (out) out->put(d, n);
    }
    void event(Event e) override
    {
        events.push_back(e);
        if (e == Event::CodeReady && ep->code()) std::memcpy(code_at_ready, ep->code(), 7);
        if (e == Event::PairingDone && ep->peer_static()) done_peer.assign(ep->peer_static(), ep->peer_static() + 32);
    }
    void message(const msg::Header& h, const uint8_t* body) override
    {
        msgs.push_back({h.type, Bytes(body, body + h.len)});
    }
    int count(Event e) const
    {
        int c = 0;
        for (Event x : events) c += (x == e);
        return c;
    }
    bool saw(Event e) const { return count(e) > 0; }
    void pump(uint32_t now)
    {
        if (!powered && hold_while_off) return;
        while (in && !in->q.empty()) {
            Bytes b = std::move(in->q.front());
            in->q.pop_front();
            if (powered) ep->on_frame(b.data(), b.size(), now);
        }
    }
    void tick(uint32_t now)
    {
        if (powered) ep->tick(now);
    }
    void power_off() { powered = false; }
    void power_on()
    {
        powered = true;
        boot();
    }
};

struct Rig {
    Wire vd, dv;  // vault -> dongle, dongle -> vault
    Node vault, dongle;
    uint32_t now = 0;
    explicit Rig(const link::Params& p = link::Params()) : vault(Role::Vault, p), dongle(Role::Dongle, p)
    {
        vault.out = &vd;
        dongle.in = &vd;
        dongle.out = &dv;
        vault.in = &dv;
    }
    void run(uint32_t ms)
    {
        const uint32_t end = now + ms;
        while (now < end) {
            for (int i = 0; i < 20; ++i) {  // deliver until quiet (a frame can trigger the answer)
                vault.pump(now);
                dongle.pump(now);
                if (vd.q.empty() && dv.q.empty()) break;
            }
            vault.tick(now);
            dongle.tick(now);
            ++now;
        }
    }
    bool linked() const
    {
        return vault.ep->state() == State::Linked && dongle.ep->state() == State::Linked;
    }
    // Run until both ends show a code (or the time is up).
    bool run_to_code(uint32_t max_ms)
    {
        const uint32_t end = now + max_ms;
        while (now < end) {
            run(1);
            if (vault.ep->state() == State::Confirming && dongle.ep->state() == State::Confirming) return true;
        }
        return false;
    }
    bool run_to_link(uint32_t max_ms)
    {
        const uint32_t end = now + max_ms;
        while (now < end) {
            run(1);
            if (linked()) return true;
        }
        return false;
    }
    // Pair both ends completely (window open on the dongle, both users confirm).
    bool pair()
    {
        dongle.ep->open_pairing(now, 60000);
        vault.ep->start_pairing(now);
        if (!run_to_code(3000)) return false;
        vault.ep->confirm_pairing(now);
        dongle.ep->confirm_pairing(now);
        run(50);
        return vault.ep->has_trusted_peer() && dongle.ep->has_trusted_peer() && vault.saw(Event::PairingDone) &&
               dongle.saw(Event::PairingDone);
    }
};

bool same(const uint8_t* a, const uint8_t* b) { return std::memcmp(a, b, 32) == 0; }

// ---------------------------------------------------------------------------------------------

void test_pairing_and_link()
{
    Rig r;
    CHECK(r.dongle.ep->state() == State::Idle);
    CHECK(!r.dongle.ep->has_trusted_peer());

    r.dongle.ep->open_pairing(r.now, 60000);
    r.vault.ep->start_pairing(r.now);
    CHECK(r.run_to_code(3000));
    CHECK(r.vault.saw(Event::PairingStarted));
    CHECK(r.dongle.saw(Event::PairingStarted));
    CHECK(r.vault.saw(Event::CodeReady) && r.dongle.saw(Event::CodeReady));
    CHECK(std::strlen(r.vault.code_at_ready) == 6);
    CHECK(std::strcmp(r.vault.code_at_ready, r.dongle.code_at_ready) == 0);
    CHECK(r.vault.ep->code() != nullptr && r.dongle.ep->code() != nullptr);

    // Nothing is stored until both users confirm.
    CHECK(!r.vault.ep->has_trusted_peer() && !r.dongle.ep->has_trusted_peer());
    r.vault.ep->confirm_pairing(r.now);
    r.run(20);
    CHECK(!r.vault.saw(Event::PairingDone) && !r.dongle.saw(Event::PairingDone));
    CHECK(r.dongle.ep->remote_confirmed() && !r.dongle.ep->local_confirmed());
    r.dongle.ep->confirm_pairing(r.now);
    r.run(20);

    CHECK(r.vault.saw(Event::PairingDone) && r.dongle.saw(Event::PairingDone));
    CHECK(r.vault.ep->state() == State::Idle && r.dongle.ep->state() == State::Idle);
    CHECK(r.vault.done_peer.size() == 32 && same(r.vault.done_peer.data(), r.dongle.key.pk));
    CHECK(r.dongle.done_peer.size() == 32 && same(r.dongle.done_peer.data(), r.vault.key.pk));
    CHECK(r.vault.ep->has_trusted_peer() && same(r.vault.ep->trusted_peer(), r.dongle.key.pk));
    CHECK(r.dongle.ep->has_trusted_peer() && same(r.dongle.ep->trusted_peer(), r.vault.key.pk));
    CHECK(!r.dongle.ep->pairing_open());
    CHECK(r.vault.ep->code() == nullptr && r.dongle.ep->code() == nullptr);

    // Session
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(2000));
    CHECK(r.vault.saw(Event::Connecting));
    CHECK(r.vault.saw(Event::Linked) && r.dongle.saw(Event::Linked));
    CHECK(r.vault.ep->peer_static() && same(r.vault.ep->peer_static(), r.dongle.key.pk));
    CHECK(r.dongle.ep->peer_static() && same(r.dongle.ep->peer_static(), r.vault.key.pk));

    // Ping / Pong keep it alive for a long time with nothing else going on.
    r.run(60000);
    CHECK(r.linked());
    CHECK(r.vault.count(Event::LinkLost) == 0 && r.dongle.count(Event::LinkLost) == 0);
    CHECK(r.vault.ep->last_rtt_ms() <= 1);

    // Application messages both ways.
    const uint8_t body[] = {1, 2, 3, 4, 5, 6, 7};
    CHECK(r.vault.ep->send_message(msg::Type::TypeKeys, 0, body, sizeof body));
    r.run(5);
    CHECK(r.dongle.msgs.size() == 1 && r.dongle.msgs[0].type == msg::Type::TypeKeys &&
          r.dongle.msgs[0].body == Bytes(body, body + sizeof body));
    const uint8_t res[] = {0, 0, 0, 0};
    CHECK(r.dongle.ep->send_message(msg::Type::Result, 0, res, sizeof res));
    r.run(5);
    CHECK(r.vault.msgs.size() == 1 && r.vault.msgs[0].type == msg::Type::Result);
    CHECK(r.vault.msgs[0].body == Bytes(res, res + sizeof res));
    // Internal messages are not the application's to send; and the largest body still fits a frame.
    CHECK(!r.vault.ep->send_message(msg::Type::Hello, 0, nullptr, 0));
    CHECK(!r.vault.ep->send_message(msg::Type::PairConfirm, 0, nullptr, 0));
    CHECK(!r.vault.ep->send_message(msg::Type::Bye, 0, nullptr, 0));
    Bytes big(msg::kMaxBody, 0xAB);
    CHECK(r.vault.ep->send_message(msg::Type::TypeKeys, 0, big.data(), big.size()));
    for (const Bytes& f : r.vd.seen) CHECK(f.size() <= link::Endpoint::kBufLen);
    r.run(5);
    CHECK(r.dongle.msgs.size() == 2 && r.dongle.msgs[1].body == big);
    big.push_back(0);
    CHECK(!r.vault.ep->send_message(msg::Type::TypeKeys, 0, big.data(), big.size()));

    // Clean goodbye.
    r.vault.ep->disconnect();
    r.run(50);
    CHECK(r.vault.ep->state() == State::Idle && r.dongle.ep->state() == State::Idle);
    CHECK(r.vault.saw(Event::LinkLost) && r.dongle.saw(Event::LinkLost));
    r.run(10000);
    CHECK(r.vault.ep->state() == State::Idle);  // it does not reconnect on its own after disconnect()
    CHECK(!r.vault.ep->send_message(msg::Type::TypeKeys, 0, body, sizeof body));
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(2000));
}

void test_no_window_no_pairing()
{
    link::Params p;
    Rig r(p);
    r.vault.ep->start_pairing(r.now);
    r.run(p.retry_ms * (p.max_pair_attempts + 2));
    CHECK(r.vault.saw(Event::PairingFailed));
    CHECK(r.vault.ep->fail_reason() == link::Fail::NoReply);
    CHECK(r.vault.ep->state() == State::Idle);
    CHECK(r.vault.ep->attempts() == p.max_pair_attempts);
    CHECK(r.vault.count(Event::PairingStarted) == p.max_pair_attempts);
    CHECK(!r.dongle.saw(Event::PairingStarted));
    CHECK(r.dv.sent == 0);  // a closed dongle says nothing at all
}

void test_window_expires()
{
    Rig r;
    r.dongle.ep->open_pairing(r.now, 5000);
    CHECK(r.dongle.ep->pairing_open());
    r.run(4900);
    CHECK(r.dongle.ep->pairing_open() && !r.dongle.saw(Event::PairingWindowClosed));
    r.run(200);
    CHECK(!r.dongle.ep->pairing_open() && r.dongle.saw(Event::PairingWindowClosed));
    // closed on request: no event, no pairing
    Rig q;
    q.dongle.ep->open_pairing(q.now, 5000);
    q.dongle.ep->close_pairing();
    q.vault.ep->start_pairing(q.now);
    q.run(3000);
    CHECK(!q.dongle.saw(Event::PairingStarted));
}

void test_window_outlives_slow_users()
{
    // The users take 50 s to compare the codes; the dongle's 5 s window must not kill the pairing.
    Rig r;
    r.dongle.ep->open_pairing(r.now, 5000);
    r.vault.ep->start_pairing(r.now);
    CHECK(r.run_to_code(3000));
    r.run(50000);
    CHECK(r.dongle.ep->state() == State::Confirming && r.vault.ep->state() == State::Confirming);
    CHECK(!r.dongle.saw(Event::PairingWindowClosed));  // the window has not "expired" under them
    r.vault.ep->confirm_pairing(r.now);
    r.dongle.ep->confirm_pairing(r.now);
    r.run(50);
    CHECK(r.vault.saw(Event::PairingDone) && r.dongle.saw(Event::PairingDone));
}

void test_rejects()
{
    {   // the vault user says no
        Rig r;
        r.dongle.ep->open_pairing(r.now, 60000);
        r.vault.ep->start_pairing(r.now);
        CHECK(r.run_to_code(3000));
        r.dongle.ep->confirm_pairing(r.now);
        r.vault.ep->reject_pairing();
        r.run(50);
        CHECK(r.vault.saw(Event::PairingRejected) && r.dongle.saw(Event::PairingRejected));
        CHECK(!r.vault.saw(Event::PairingDone) && !r.dongle.saw(Event::PairingDone));
        CHECK(!r.vault.ep->has_trusted_peer() && !r.dongle.ep->has_trusted_peer());
        CHECK(r.vault.ep->state() == State::Idle && r.dongle.ep->state() == State::Idle);
        CHECK(!r.dongle.ep->pairing_open());
    }
    {   // the dongle user says no
        Rig r;
        r.dongle.ep->open_pairing(r.now, 60000);
        r.vault.ep->start_pairing(r.now);
        CHECK(r.run_to_code(3000));
        r.vault.ep->confirm_pairing(r.now);
        r.dongle.ep->reject_pairing();
        r.run(50);
        CHECK(r.vault.saw(Event::PairingRejected) && r.dongle.saw(Event::PairingRejected));
        CHECK(!r.vault.ep->has_trusted_peer() && !r.dongle.ep->has_trusted_peer());
    }
    {   // nobody answers in time
        link::Params p;
        p.confirm_timeout_ms = 10000;
        Rig r(p);
        r.dongle.ep->open_pairing(r.now, 60000);
        r.vault.ep->start_pairing(r.now);
        CHECK(r.run_to_code(3000));
        r.vault.ep->confirm_pairing(r.now);  // only one side confirms
        r.run(11000);
        CHECK(r.vault.saw(Event::PairingRejected) && r.dongle.saw(Event::PairingRejected));
        CHECK(!r.vault.ep->has_trusted_peer() && !r.dongle.ep->has_trusted_peer());
    }
    {   // a second pairing works after a rejected one, with a new code
        Rig r;
        r.dongle.ep->open_pairing(r.now, 60000);
        r.vault.ep->start_pairing(r.now);
        CHECK(r.run_to_code(3000));
        const Bytes first(reinterpret_cast<const uint8_t*>(r.vault.code_at_ready),
                          reinterpret_cast<const uint8_t*>(r.vault.code_at_ready) + 7);
        r.vault.ep->reject_pairing();
        r.run(50);
        CHECK(r.pair());
        CHECK(std::memcmp(first.data(), r.vault.code_at_ready, 7) != 0);  // 1e-6 chance of a false alarm
    }
}

void test_link_needs_the_right_key()
{
    Rig r;
    CHECK(r.pair());

    // 1. A vault with another key cannot link to this dongle.
    Rig other;
    Node& stranger = other.vault;
    other.dongle.power_off();
    // Build the stranger by hand: it knows the dongle's public key, but it is not the paired vault.
    Wire sv, vs;
    stranger.out = &sv;
    stranger.in = &vs;
    stranger.ep->set_trusted_peer(r.dongle.key.pk);
    CHECK(stranger.ep->connect(0));
    Node& real_dongle = r.dongle;
    Wire* old_in = real_dongle.in;
    Wire* old_out = real_dongle.out;
    real_dongle.in = &sv;
    real_dongle.out = &vs;
    for (uint32_t t = 0; t < 5000; ++t) {
        stranger.pump(t);
        real_dongle.pump(t);
        stranger.tick(t);
        real_dongle.tick(t);
    }
    CHECK(stranger.ep->state() != State::Linked);
    CHECK(real_dongle.ep->state() != State::Linked);
    CHECK(real_dongle.ep->fail_reason() == link::Fail::WrongPeer);
    CHECK(vs.sent == 0);  // no answer to a stranger
    real_dongle.in = old_in;
    real_dongle.out = old_out;

    // 2. The paired vault still can.
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
}

void test_unpaired_dongle_ignores_sessions()
{
    Rig r;
    // The vault believes in a dongle key; the dongle was never paired (or forgot).
    r.vault.ep->set_trusted_peer(r.dongle.key.pk);
    CHECK(r.vault.ep->connect(r.now));
    r.run(6000);
    CHECK(r.dongle.ep->state() == State::Idle);
    CHECK(r.dv.sent == 0);
    CHECK(!r.vault.saw(Event::Linked));
    CHECK(r.vault.count(Event::Retry) >= 4);  // it keeps trying, once per second
}

void test_forgotten_dongle_and_repair()
{
    Rig r;
    CHECK(r.pair());
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
    r.dongle.ep->forget_peer();
    CHECK(r.dongle.saw(Event::LinkLost));
    CHECK(!r.dongle.ep->has_trusted_peer());
    r.run(10000);
    CHECK(!r.linked());
    // pair again with the same devices
    r.vault.ep->start_pairing(r.now);
    r.dongle.ep->open_pairing(r.now, 60000);
    CHECK(r.run_to_code(3000));
    r.vault.ep->confirm_pairing(r.now);
    r.dongle.ep->confirm_pairing(r.now);
    r.run(50);
    CHECK(r.dongle.ep->has_trusted_peer());
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
}

void test_dongle_reboots()
{
    Rig r;
    CHECK(r.pair());
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
    const uint8_t vault_pk[32] = {};
    (void)vault_pk;

    r.dongle.power_off();
    r.run(7000);  // silence: the vault notices
    CHECK(r.vault.saw(Event::LinkLost));
    CHECK(r.vault.ep->state() == State::Connecting);  // and is already trying again
    r.dongle.power_on();                               // new object, no memory of anything...
    CHECK(!r.dongle.ep->has_trusted_peer());
    r.dongle.ep->set_trusted_peer(r.vault.key.pk);     // ...except what the application stored
    CHECK(r.run_to_link(5000));
    CHECK(r.vault.count(Event::Linked) == 2);
}

void test_vault_reboots()
{
    Rig r;
    CHECK(r.pair());
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));

    r.vault.power_off();
    r.run(100);
    r.vault.power_on();
    r.vault.ep->set_trusted_peer(r.dongle.key.pk);
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
    // the dongle still believed in the old session: it must report that it ended and a new one began
    CHECK(r.dongle.count(Event::LinkLost) == 1);
    CHECK(r.dongle.count(Event::Linked) == 2);
}

void test_dongle_boots_late()
{
    Rig r;
    CHECK(r.pair());
    r.dongle.power_off();
    r.dongle.hold_while_off = true;  // the wire keeps what the vault sends meanwhile (like a pipe)
    CHECK(r.vault.ep->connect(r.now));
    r.run(3500);
    CHECK(r.vd.q.size() >= 3);
    r.dongle.power_on();
    r.dongle.hold_while_off = false;
    r.dongle.ep->set_trusted_peer(r.vault.key.pk);
    // It answers every queued first message: the vault gets replies to attempts that no longer exist.
    CHECK(r.run_to_link(8000));
}

void test_lost_frames_break_and_heal()
{
    Rig r;
    CHECK(r.pair());
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
    // One lost frame in the middle of a session makes the counters disagree for good...
    bool dropped_one = false;
    r.vd.filter = [&](Bytes&) {
        if (!dropped_one) {
            dropped_one = true;
            return false;
        }
        return true;
    };
    r.run(9000);  // a Ping is lost; the next ones no longer decrypt; both ends fall silent
    CHECK(dropped_one);
    CHECK(r.dongle.saw(Event::LinkLost) && r.vault.saw(Event::LinkLost));
    // ...and the endpoints repair it themselves with a fresh handshake.
    CHECK(r.run_to_link(10000));
    CHECK(r.vault.count(Event::Linked) == 2 && r.dongle.count(Event::Linked) == 2);
    const size_t before = r.dongle.msgs.size();
    const uint8_t body[] = {9, 9, 9};
    CHECK(r.vault.ep->send_message(msg::Type::TypeKeys, 0, body, sizeof body));
    r.run(5);
    CHECK(r.dongle.msgs.size() == before + 1);
}

void test_dead_wire_then_back()
{
    Rig r;
    CHECK(r.pair());
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
    r.vd.filter = [](Bytes&) { return false; };
    r.dv.filter = [](Bytes&) { return false; };
    r.run(15000);
    CHECK(r.vault.saw(Event::LinkLost) && r.dongle.saw(Event::LinkLost));
    CHECK(!r.linked());
    r.vd.filter = nullptr;
    r.dv.filter = nullptr;
    CHECK(r.run_to_link(5000));
}

void test_strangers_cannot_disturb()
{
    Rig r;
    CHECK(r.pair());
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
    uint32_t seed = 12345;
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<uint8_t>(seed >> 24);
    };
    const size_t lens[] = {1, 5, 22, 24, 32, 47, 48, 49, 63, 64, 65, 95, 96, 97, 200, 255, 256};
    for (int round = 0; round < 40; ++round) {
        for (size_t n : lens) {
            Bytes junk(n);
            for (auto& b : junk) b = rnd();
            r.dongle.ep->on_frame(junk.data(), junk.size(), r.now);
            r.vault.ep->on_frame(junk.data(), junk.size(), r.now);
        }
        r.run(25);
    }
    r.dongle.ep->on_frame(nullptr, 0, r.now);
    r.run(2000);
    CHECK(r.linked());
    CHECK(r.vault.count(Event::LinkLost) == 0 && r.dongle.count(Event::LinkLost) == 0);
    const uint8_t body[] = {1};
    CHECK(r.vault.ep->send_message(msg::Type::TypeKeys, 0, body, 1));
    r.run(5);
    CHECK(r.dongle.msgs.size() == 1);

    // a replayed copy of an old frame is rejected too
    Bytes old = r.vd.seen[r.vd.seen.size() - 1];
    const int linked_before = r.dongle.count(Event::Linked);
    r.dongle.ep->on_frame(old.data(), old.size(), r.now);
    r.run(100);
    CHECK(r.linked() && r.dongle.count(Event::Linked) == linked_before);
    CHECK(r.dongle.msgs.size() == 1);
}

void test_strangers_during_pairing()
{
    // Junk while the window is open and a pairing is running must not derail it.
    Rig r;
    r.dongle.ep->open_pairing(r.now, 60000);
    r.vault.ep->start_pairing(r.now);
    uint32_t seed = 777;
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<uint8_t>(seed >> 24);
    };
    for (int i = 0; i < 20; ++i) {
        for (size_t n : {size_t(48), size_t(64), size_t(96)}) {
            Bytes junk(n);
            for (auto& b : junk) b = rnd();
            r.vault.ep->on_frame(junk.data(), junk.size(), r.now);
            if (n != 64) r.dongle.ep->on_frame(junk.data(), junk.size(), r.now);
        }
        r.run(1);
    }
    CHECK(r.run_to_code(8000));
    CHECK(std::strcmp(r.vault.code_at_ready, r.dongle.code_at_ready) == 0);
}

void test_lossy_pairing()
{
    // Frames vanish at random during pairing. It may take several attempts, but when both ends do
    // show a code it is the same one, and a confirmed pairing always leaves both ends in agreement.
    int both_done = 0, one_sided = 0, never = 0;
    for (int trial = 0; trial < 80; ++trial) {
        Rig r;
        uint32_t seed = 1000u + static_cast<uint32_t>(trial);
        auto rnd = [&]() {
            seed = seed * 1664525u + 1013904223u;
            return seed >> 8;
        };
        auto lossy = [&](Bytes&) { return rnd() % 100 >= 25; };  // 25 % of the frames are lost
        r.vd.filter = lossy;
        r.dv.filter = lossy;
        r.dongle.ep->open_pairing(r.now, 120000);
        r.vault.ep->start_pairing(r.now);
        bool code = false;
        for (int t = 0; t < 12000 && !code; ++t) {
            r.run(1);
            code = r.vault.ep->state() == State::Confirming && r.dongle.ep->state() == State::Confirming;
            if (r.vault.saw(Event::PairingFailed)) break;
        }
        if (!code) {
            ++never;
            continue;
        }
        CHECK(std::strcmp(r.vault.ep->code(), r.dongle.ep->code()) == 0);
        r.vault.ep->confirm_pairing(r.now);
        r.dongle.ep->confirm_pairing(r.now);
        r.run(200);
        const bool v = r.vault.saw(Event::PairingDone), d = r.dongle.saw(Event::PairingDone);
        if (v && d) {
            ++both_done;
            CHECK(same(r.vault.ep->trusted_peer(), r.dongle.key.pk));
            CHECK(same(r.dongle.ep->trusted_peer(), r.vault.key.pk));
        } else {
            ++one_sided;  // a lost PairConfirm: that pairing times out instead of completing
            CHECK(!(v && !d && r.dongle.ep->state() == State::Linked));
        }
    }
    std::printf("  lossy pairing: %d both done, %d unfinished after confirm, %d never reached a code\n", both_done,
                one_sided, never);
    CHECK(both_done >= 30);
}

void test_corrupting_wire_never_mismatches()
{
    // Bit flips (the real wire has a CRC that turns these into losses; this is the worst case).
    int agreed = 0;
    for (int trial = 0; trial < 80; ++trial) {
        Rig r;
        uint32_t seed = 5000u + static_cast<uint32_t>(trial);
        auto rnd = [&]() {
            seed = seed * 1664525u + 1013904223u;
            return seed >> 8;
        };
        auto flip = [&](Bytes& b) {
            if (rnd() % 100 < 15 && !b.empty()) b[rnd() % b.size()] ^= static_cast<uint8_t>(1u << (rnd() % 8));
            return true;
        };
        r.vd.filter = flip;
        r.dv.filter = flip;
        r.dongle.ep->open_pairing(r.now, 120000);
        r.vault.ep->start_pairing(r.now);
        for (int t = 0; t < 15000; ++t) {
            r.run(1);
            if (r.vault.ep->state() == State::Confirming && r.dongle.ep->state() == State::Confirming) break;
        }
        if (r.vault.ep->state() == State::Confirming && r.dongle.ep->state() == State::Confirming) {
            CHECK(std::strcmp(r.vault.ep->code(), r.dongle.ep->code()) == 0);
            ++agreed;
        }
    }
    CHECK(agreed >= 20);
}

void test_man_in_the_middle()
{
    // Mallory runs a pairing with each side separately and relays nothing: the two users see
    // different codes (equal only by a 1-in-a-million chance).
    int equal = 0;
    for (int trial = 0; trial < 60; ++trial) {
        link::Params p;
        Node vault(Role::Vault, p), mal_d(Role::Dongle, p), mal_v(Role::Vault, p), dongle(Role::Dongle, p);
        Wire a, b, c, d;
        vault.out = &a;   mal_d.in = &a;
        mal_d.out = &b;   vault.in = &b;
        mal_v.out = &c;   dongle.in = &c;
        dongle.out = &d;  mal_v.in = &d;
        dongle.ep->open_pairing(0, 60000);
        mal_d.ep->open_pairing(0, 60000);
        vault.ep->start_pairing(0);
        mal_v.ep->start_pairing(0);
        for (uint32_t t = 0; t < 3000; ++t) {
            for (int i = 0; i < 10; ++i) {
                vault.pump(t); mal_d.pump(t); mal_v.pump(t); dongle.pump(t);
            }
            vault.tick(t); mal_d.tick(t); mal_v.tick(t); dongle.tick(t);
            if (vault.ep->state() == State::Confirming && dongle.ep->state() == State::Confirming) break;
        }
        CHECK(vault.ep->state() == State::Confirming && dongle.ep->state() == State::Confirming);
        if (vault.ep->code() && dongle.ep->code() && std::strcmp(vault.ep->code(), dongle.ep->code()) == 0) ++equal;
    }
    CHECK(equal <= 1);
}

void test_version_mismatch_and_state_queries()
{
    Rig r;
    CHECK(r.vault.ep->role() == Role::Vault && r.dongle.ep->role() == Role::Dongle);
    CHECK(std::strcmp(link::state_name(State::Linked), "linked") == 0);
    CHECK(std::strcmp(link::event_name(Event::CodeReady), "code ready") == 0);
    CHECK(std::strcmp(link::fail_name(link::Fail::WrongPeer), "unknown peer") == 0);
    // commands on the wrong role do nothing
    r.dongle.ep->start_pairing(0);
    CHECK(r.dongle.ep->state() == State::Idle);
    r.vault.ep->open_pairing(0, 1000);
    CHECK(!r.vault.ep->pairing_open());
    CHECK(!r.dongle.ep->connect(0));
    CHECK(!r.vault.ep->connect(0));  // no trusted peer yet
    r.vault.ep->confirm_pairing(0);  // nothing to confirm
    CHECK(!r.vault.ep->local_confirmed());
}

// A "vault" written by hand against the Noise API: opens an IK session to the dongle without using
// link::Endpoint, so a test can say things Endpoint never would.
struct RawVault {
    noise::Transport tr;
    bool open(Rig& r)
    {
        noise::HandshakeState hs;
        noise::HandshakeState::Config c;
        uint8_t prologue[pairing::kPrologueLen];
        pairing::make_prologue(prologue);
        c.pattern = noise::Pattern::IK;
        c.role = noise::Role::Initiator;
        c.prologue = prologue;
        c.prologue_len = sizeof prologue;
        c.s = &r.vault.key;
        c.rs = r.dongle.key.pk;
        uint8_t m1[256], pt[256];
        size_t n1 = 0, pn = 0;
        if (hs.init(c) != noise::Status::Ok || hs.write_message(nullptr, 0, m1, sizeof m1, &n1) != noise::Status::Ok) {
            return false;
        }
        r.dv.q.clear();
        r.dongle.ep->on_frame(m1, n1, 0);
        if (r.dv.q.size() != 1) return false;
        const Bytes reply = r.dv.q.front();
        r.dv.q.clear();
        return hs.read_message(reply.data(), reply.size(), pt, sizeof pt, &pn) == noise::Status::Ok &&
               hs.split(&tr) == noise::Status::Ok;
    }
    Bytes seal(msg::Type t, const uint8_t* body, size_t n)
    {
        uint8_t plain[256], out[256];
        size_t np = 0, no = 0;
        if (!msg::encode(t, 0, 0, body, n, plain, sizeof plain, &np) ||
            tr.seal(plain, np, out, sizeof out, &no) != noise::Status::Ok) {
            return Bytes();
        }
        return Bytes(out, out + no);
    }
    bool opens_as(const Bytes& frame, msg::Type t)
    {
        uint8_t out[256];
        size_t on = 0;
        msg::Header h;
        const uint8_t* body = nullptr;
        return tr.open(frame.data(), frame.size(), out, sizeof out, &on) == noise::Status::Ok &&
               msg::parse(out, on, &h, &body) == msg::ParseStatus::Ok && h.type == t;
    }
};

void test_version_mismatch()
{
    // The vault says Hello with a protocol version the dongle does not speak: the dongle must refuse
    // (Bye) and not link.
    Rig r;
    CHECK(r.pair());
    RawVault v;
    CHECK(v.open(r));
    const uint8_t hello_body[2] = {static_cast<uint8_t>(msg::kProtocolVersion + 1), 0};
    const Bytes hello = v.seal(msg::Type::Hello, hello_body, 2);
    CHECK(!hello.empty());
    r.dongle.ep->on_frame(hello.data(), hello.size(), 1);
    CHECK(r.dongle.ep->state() == State::Idle);
    CHECK(r.dongle.ep->fail_reason() == link::Fail::Version);
    CHECK(!r.dongle.saw(Event::Linked));
    CHECK(r.dv.q.size() == 1);  // a Bye, sealed under the session
    CHECK(v.opens_as(r.dv.q.front(), msg::Type::Bye));
}

void test_no_messages_before_hello()
{
    // An authentic vault that skips Hello and goes straight to TypeKeys gets nothing delivered, and no link.
    Rig r;
    CHECK(r.pair());
    RawVault v;
    CHECK(v.open(r));
    const uint8_t body[7] = {1, 0, 0, 0, 0, 0, 0};
    const Bytes keys = v.seal(msg::Type::TypeKeys, body, sizeof body);
    CHECK(!keys.empty());
    r.dongle.ep->on_frame(keys.data(), keys.size(), 1);
    CHECK(r.dongle.msgs.empty());
    CHECK(r.dongle.ep->state() == State::Connecting);
    CHECK(!r.dongle.saw(Event::Linked));
    // and with a proper Hello it links
    const uint8_t hb[2] = {msg::kProtocolVersion, 0};
    const Bytes hello = v.seal(msg::Type::Hello, hb, 2);
    r.dv.q.clear();
    r.dongle.ep->on_frame(hello.data(), hello.size(), 2);
    CHECK(r.dongle.ep->state() == State::Linked);
    CHECK(r.dv.q.size() == 1 && v.opens_as(r.dv.q.front(), msg::Type::HelloAck));
}

void test_pairing_while_linked()
{
    // Pairing a new vault while the dongle is serving the old one ends the old session.
    Rig r;
    CHECK(r.pair());
    CHECK(r.vault.ep->connect(r.now));
    CHECK(r.run_to_link(3000));
    r.dongle.ep->open_pairing(r.now, 60000);
    CHECK(r.dongle.saw(Event::LinkLost));
    CHECK(r.dongle.ep->state() == State::Idle);
    r.run(7000);
    CHECK(r.vault.saw(Event::LinkLost));
}

}  // namespace

int main()
{
    CHECK(crypto::selftest() == nullptr);
    test_pairing_and_link();
    test_no_window_no_pairing();
    test_window_expires();
    test_window_outlives_slow_users();
    test_rejects();
    test_link_needs_the_right_key();
    test_unpaired_dongle_ignores_sessions();
    test_forgotten_dongle_and_repair();
    test_dongle_reboots();
    test_vault_reboots();
    test_dongle_boots_late();
    test_lost_frames_break_and_heal();
    test_dead_wire_then_back();
    test_strangers_cannot_disturb();
    test_strangers_during_pairing();
    test_lossy_pairing();
    test_corrupting_wire_never_mismatches();
    test_man_in_the_middle();
    test_version_mismatch_and_state_queries();
    test_version_mismatch();
    test_no_messages_before_hello();
    test_pairing_while_linked();
    std::printf("link: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
