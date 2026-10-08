// Protocol-level tests: sizes promised by the specification, mutual authentication, tamper /
// replay / reorder rejection, low-order DH points, nonce exhaustion, the pairing code (SAS),
// and the whole byte path (message -> AEAD -> frame -> chunked stream -> frame -> AEAD -> message).
#include "kkproto/frames.hpp"
#include "kkproto/messages.hpp"
#include "kkproto/noise.hpp"
#include "kkproto/pairing.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstring>

using namespace kk;
using namespace kk::noise;

static KeyPair gen()
{
    KeyPair k{};
    CHECK(crypto::x25519_keygen(k.sk, k.pk));
    return k;
}
static Bytes prologue()
{
    Bytes p(pairing::kPrologueLen);
    pairing::make_prologue(p.data());
    return p;
}

struct Mutation { int msg = -1; size_t byte = 0; uint8_t mask = 0; };
struct Outcome {
    bool ok = false;
    int stage = -1;               // index of the first failing message, or -1
    Status status = Status::Ok;
    Bytes wire[3];
    uint8_t hv[32] = {}, hd[32] = {};
    Transport tv, td;
    uint8_t rs_seen_by_v[32] = {}, rs_seen_by_d[32] = {};
};

// Vault = initiator, dongle = responder. For IK the vault must know the dongle's static key (v_knows_d).
static Outcome run_handshake(Pattern p, const KeyPair& vs, const KeyPair& ds, const uint8_t* v_knows_d,
                             const Bytes& prol_v, const Bytes& prol_d, const Mutation& mu = Mutation(),
                             const Bytes pay[3] = nullptr)
{
    Outcome o;
    HandshakeState v, d;
    HandshakeState::Config cv, cd;
    cv.pattern = cd.pattern = p;
    cv.role = Role::Initiator; cd.role = Role::Responder;
    cv.prologue = prol_v.data(); cv.prologue_len = prol_v.size();
    cd.prologue = prol_d.data(); cd.prologue_len = prol_d.size();
    cv.s = &vs; cd.s = &ds;
    cv.rs = v_knows_d;
    if (v.init(cv) != Status::Ok || d.init(cd) != Status::Ok) { o.stage = 0; return o; }
    const size_t n_msgs = (p == Pattern::XX) ? 3 : 2;
    Bytes empty;
    for (size_t i = 0; i < n_msgs; ++i) {
        HandshakeState& tx = (i % 2 == 0) ? v : d;
        HandshakeState& rx = (i % 2 == 0) ? d : v;
        const Bytes& pl = pay != nullptr ? pay[i] : empty;
        Bytes wire(400), back(400);
        size_t wn = 0, bn = 0;
        Status s = tx.write_message(pl.data(), pl.size(), wire.data(), wire.size(), &wn);
        if (s != Status::Ok) { o.stage = static_cast<int>(i); o.status = s; return o; }
        wire.resize(wn);
        if (mu.msg == static_cast<int>(i) && mu.byte < wire.size()) wire[mu.byte] ^= mu.mask;
        o.wire[i] = wire;
        s = rx.read_message(wire.data(), wire.size(), back.data(), back.size(), &bn);
        if (s != Status::Ok) { o.stage = static_cast<int>(i); o.status = s; return o; }
        if (pay != nullptr) { back.resize(bn); if (back != pl && mu.msg < 0) { o.stage = static_cast<int>(i); o.status = Status::BadMessage; return o; } }
    }
    if (!v.complete() || !d.complete()) { o.stage = 99; return o; }
    std::memcpy(o.hv, v.handshake_hash(), 32);
    std::memcpy(o.hd, d.handshake_hash(), 32);
    if (v.remote_static()) std::memcpy(o.rs_seen_by_v, v.remote_static(), 32);
    if (d.remote_static()) std::memcpy(o.rs_seen_by_d, d.remote_static(), 32);
    if (v.split(&o.tv) != Status::Ok || d.split(&o.td) != Status::Ok) { o.stage = 98; return o; }
    o.ok = true;
    return o;
}

// A transport round trip proves both sides hold matching keys.
static bool transport_works(Outcome& o)
{
    const uint8_t a[] = {1, 2, 3, 4, 5};
    uint8_t ct[64], pt[64];
    size_t cn = 0, pn = 0;
    if (o.tv.seal(a, sizeof a, ct, sizeof ct, &cn) != Status::Ok) return false;
    if (o.td.open(ct, cn, pt, sizeof pt, &pn) != Status::Ok || pn != sizeof a || std::memcmp(pt, a, pn) != 0) return false;
    if (o.td.seal(a, sizeof a, ct, sizeof ct, &cn) != Status::Ok) return false;
    return o.tv.open(ct, cn, pt, sizeof pt, &pn) == Status::Ok && pn == sizeof a && std::memcmp(pt, a, pn) == 0;
}

static void test_sizes_and_pairing()
{
    const KeyPair vs = gen(), ds = gen();
    const Bytes pr = prologue();
    // Specification numbers: XX = 32 / 96 / 64 bytes, IK = 96 / 48 bytes, all with empty payloads.
    Outcome xx = run_handshake(Pattern::XX, vs, ds, nullptr, pr, pr);
    CHECK(xx.ok);
    CHECK(xx.wire[0].size() == 32 && xx.wire[1].size() == 96 && xx.wire[2].size() == 64);
    CHECK(std::memcmp(xx.hv, xx.hd, 32) == 0);
    CHECK(std::memcmp(xx.rs_seen_by_v, ds.pk, 32) == 0 && std::memcmp(xx.rs_seen_by_d, vs.pk, 32) == 0);
    CHECK(transport_works(xx));
    // Pairing code is identical on both sides and 6 digits.
    const uint32_t sv = pairing::sas_code(xx.hv), sd = pairing::sas_code(xx.hd);
    char t[7];
    pairing::sas_text(sv, t);
    CHECK(sv == sd && sv < 1000000 && std::strlen(t) == 6);
    // With payloads (version info rides in the handshake): same structure, payload sizes added.
    const Bytes pay[3] = {Bytes{}, Bytes{1, 2, 3, 4}, Bytes{9, 8}};
    Outcome xp = run_handshake(Pattern::XX, vs, ds, nullptr, pr, pr, Mutation(), pay);
    CHECK(xp.ok && xp.wire[1].size() == 96 + 4 && xp.wire[2].size() == 64 + 2);
    // Session: IK with the keys learned during pairing.
    Outcome ik = run_handshake(Pattern::IK, vs, ds, ds.pk, pr, pr);
    CHECK(ik.ok);
    CHECK(ik.wire[0].size() == 96 && ik.wire[1].size() == 48);
    CHECK(std::memcmp(ik.rs_seen_by_d, vs.pk, 32) == 0); // the dongle learns WHO is talking and can compare to its stored key
    CHECK(transport_works(ik));
    // Forward secrecy shape: two sessions between the same peers share nothing.
    Outcome ik2 = run_handshake(Pattern::IK, vs, ds, ds.pk, pr, pr);
    CHECK(ik2.ok && std::memcmp(ik.hv, ik2.hv, 32) != 0 && ik.wire[0] != ik2.wire[0]);
}

static void test_authentication_failures()
{
    const KeyPair vs = gen(), ds = gen(), other = gen();
    const Bytes pr = prologue();
    Bytes bad = pr;
    bad.back() ^= 0x01;
    // Prologue mismatch is caught: immediately by IK, at the second message by XX.
    Outcome a = run_handshake(Pattern::IK, vs, ds, ds.pk, pr, bad);
    CHECK(!a.ok && a.stage == 0 && a.status == Status::DecryptFailed);
    Outcome b = run_handshake(Pattern::XX, vs, ds, nullptr, pr, bad);
    CHECK(!b.ok && b.stage == 1 && b.status == Status::DecryptFailed);
    // IK: the vault believes the dongle has another key -> the dongle cannot decrypt message 1.
    Outcome c = run_handshake(Pattern::IK, vs, ds, other.pk, pr, pr);
    CHECK(!c.ok && c.stage == 0 && c.status == Status::DecryptFailed);
    // IK with an unpaired vault is cryptographically valid; the APPLICATION must compare the key.
    Outcome d = run_handshake(Pattern::IK, other, ds, ds.pk, pr, pr);
    CHECK(d.ok && std::memcmp(d.rs_seen_by_d, vs.pk, 32) != 0 && std::memcmp(d.rs_seen_by_d, other.pk, 32) == 0);
}

// Flip every single bit of every handshake message, one run per bit: none may yield a handshake
// that both sides finish with matching state.
static void test_every_bit_of_every_handshake_message()
{
    const KeyPair vs = gen(), ds = gen();
    const Bytes pr = prologue();
    size_t runs = 0, escaped = 0;
    for (Pattern p : {Pattern::XX, Pattern::IK}) {
        const size_t n_msgs = p == Pattern::XX ? 3 : 2;
        Outcome base = run_handshake(p, vs, ds, p == Pattern::IK ? ds.pk : nullptr, pr, pr);
        for (size_t m = 0; m < n_msgs; ++m) {
            for (size_t byte = 0; byte < base.wire[m].size(); ++byte) {
                for (int bit = 0; bit < 8; ++bit) {
                    Mutation mu;
                    mu.msg = static_cast<int>(m); mu.byte = byte; mu.mask = static_cast<uint8_t>(1u << bit);
                    Outcome o = run_handshake(p, vs, ds, p == Pattern::IK ? ds.pk : nullptr, pr, pr, mu);
                    ++runs;
                    if (o.ok && std::memcmp(o.hv, o.hd, 32) == 0 && transport_works(o)) ++escaped;
                }
            }
        }
    }
    CHECK_MSG(escaped == 0, "%zu of %zu single-bit tamperings were not detected", escaped, runs);
    std::printf("  single-bit tamper runs: %zu, undetected: %zu\n", runs, escaped);
}

static void test_transport_rules()
{
    const KeyPair vs = gen(), ds = gen();
    const Bytes pr = prologue();
    Outcome o = run_handshake(Pattern::IK, vs, ds, ds.pk, pr, pr);
    CHECK(o.ok);
    const uint8_t m1[] = {'o', 'n', 'e'}, m2[] = {'t', 'w', 'o'}, m3[] = {'t', 'h', 'r'};
    uint8_t c1[64], c2[64], c3[64], out[64];
    size_t n1 = 0, n2 = 0, n3 = 0, on = 0;
    CHECK(o.tv.seal(m1, 3, c1, sizeof c1, &n1) == Status::Ok && n1 == 3 + 16);
    CHECK(o.tv.seal(m2, 3, c2, sizeof c2, &n2) == Status::Ok);
    CHECK(o.tv.seal(m3, 3, c3, sizeof c3, &n3) == Status::Ok);
    // Out of order: message 2 first must fail, and must NOT consume the counter...
    CHECK(o.td.open(c2, n2, out, sizeof out, &on) == Status::DecryptFailed);
    // ...so the in-order stream still works.
    CHECK(o.td.open(c1, n1, out, sizeof out, &on) == Status::Ok && on == 3 && std::memcmp(out, m1, 3) == 0);
    // Replay of an already accepted message fails.
    CHECK(o.td.open(c1, n1, out, sizeof out, &on) == Status::DecryptFailed);
    // A dropped message (c2 skipped) breaks the stream at c3.
    CHECK(o.td.open(c3, n3, out, sizeof out, &on) == Status::DecryptFailed);
    CHECK(o.td.open(c2, n2, out, sizeof out, &on) == Status::Ok);
    CHECK(o.td.open(c3, n3, out, sizeof out, &on) == Status::Ok);
    // Truncated below the tag size, empty, and a message reflected back to its sender.
    CHECK(o.tv.open(c1, 15, out, sizeof out, &on) == Status::DecryptFailed);
    CHECK(o.tv.open(c1, 0, out, sizeof out, &on) == Status::DecryptFailed);
    uint8_t c4[64]; size_t n4 = 0;
    CHECK(o.tv.seal(m1, 3, c4, sizeof c4, &n4) == Status::Ok);
    CHECK(o.tv.open(c4, n4, out, sizeof out, &on) == Status::DecryptFailed); // its own direction's key is not its receive key
    // Every single-bit flip of a transport message is rejected, and the stream survives them all.
    CHECK(o.td.open(c4, n4, out, sizeof out, &on) == Status::Ok);
    uint8_t c5[64]; size_t n5 = 0;
    CHECK(o.tv.seal(m1, 3, c5, sizeof c5, &n5) == Status::Ok);
    size_t bad = 0;
    for (size_t i = 0; i < n5 * 8; ++i) {
        c5[i / 8] ^= static_cast<uint8_t>(1u << (i % 8));
        if (o.td.open(c5, n5, out, sizeof out, &on) != Status::DecryptFailed) ++bad;
        c5[i / 8] ^= static_cast<uint8_t>(1u << (i % 8));
    }
    CHECK(bad == 0);
    CHECK(o.td.open(c5, n5, out, sizeof out, &on) == Status::Ok); // unmodified message still accepted afterwards
    // Output buffer too small is reported, not truncated.
    uint8_t c6[64]; size_t n6 = 0;
    CHECK(o.tv.seal(m1, 3, c6, sizeof c6, &n6) == Status::Ok);
    uint8_t tiny[2];
    CHECK(o.td.open(c6, n6, tiny, sizeof tiny, &on) == Status::BufferTooSmall);
    CHECK(o.tv.seal(m1, 3, tiny, sizeof tiny, &n6) == Status::BufferTooSmall);
}

static void test_low_order_points_and_misuse()
{
    const KeyPair vs = gen(), ds = gen();
    const Bytes pr = prologue();
    // Known low-order X25519 public keys (libsodium's blocklist): the DH output is all-zero.
    const char* low[] = {"0000000000000000000000000000000000000000000000000000000000000000",
                         "0100000000000000000000000000000000000000000000000000000000000000",
                         "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
                         "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157"};
    for (const char* hex : low) {
        const Bytes e = from_hex(hex);
        HandshakeState d;
        HandshakeState::Config cd;
        cd.pattern = Pattern::XX; cd.role = Role::Responder;
        cd.prologue = pr.data(); cd.prologue_len = pr.size(); cd.s = &ds;
        CHECK(d.init(cd) == Status::Ok);
        uint8_t payload[8], out[200];
        size_t pn = 0, on = 0;
        CHECK(d.read_message(e.data(), e.size(), payload, sizeof payload, &pn) == Status::Ok); // message 1 has no DH
        const Status s = d.write_message(nullptr, 0, out, sizeof out, &on);                     // message 2 does: ee
        CHECK_MSG(s == Status::BadDhResult, "low-order point %.8s... gave %s", hex, status_name(s));
        CHECK(d.write_message(nullptr, 0, out, sizeof out, &on) == Status::WrongState);       // a failed handshake stays failed
    }
    // Nonce exhaustion: 2^64-1 is reserved.
    CipherState c;
    uint8_t key[32] = {7}, out[64];
    size_t on = 0;
    c.init_key(key);
    c.set_nonce(UINT64_MAX - 1);
    CHECK(c.encrypt(nullptr, 0, key, 4, out, sizeof out, &on) == Status::Ok);
    CHECK(c.encrypt(nullptr, 0, key, 4, out, sizeof out, &on) == Status::NonceExhausted);
    // State-machine misuse.
    HandshakeState v;
    HandshakeState::Config cv;
    cv.pattern = Pattern::XX; cv.role = Role::Initiator; cv.prologue = pr.data(); cv.prologue_len = pr.size(); cv.s = &vs;
    CHECK(v.init(cv) == Status::Ok);
    uint8_t buf[300]; size_t bn = 0;
    CHECK(v.read_message(buf, 10, buf, sizeof buf, &bn) == Status::WrongState); // initiator speaks first
    Transport t;
    CHECK(v.split(&t) == Status::WrongState);                                  // not finished
    CHECK(v.write_message(nullptr, 0, buf, 8, &bn) == Status::BufferTooSmall);  // 32-byte ephemeral does not fit
    CHECK(v.write_message(nullptr, 0, buf, sizeof buf, &bn) == Status::WrongState); // and the failure is sticky
    HandshakeState bad_cfg;
    HandshakeState::Config ik_no_rs;
    ik_no_rs.pattern = Pattern::IK; ik_no_rs.role = Role::Initiator; ik_no_rs.s = &vs;
    CHECK(bad_cfg.init(ik_no_rs) == Status::BadArgument);                       // IK initiator needs the peer key
    HandshakeState no_s;
    HandshakeState::Config ns;
    CHECK(no_s.init(ns) == Status::BadArgument);
}

static void test_sas()
{
    const Bytes pr = prologue();
    // Honest pairs always agree.
    for (int i = 0; i < 200; ++i) {
        const KeyPair vs = gen(), ds = gen();
        Outcome o = run_handshake(Pattern::XX, vs, ds, nullptr, pr, pr);
        CHECK(o.ok && pairing::sas_code(o.hv) == pairing::sas_code(o.hd));
    }
    // A man in the middle runs one handshake with each side and gets different hashes, hence
    // (almost always) different codes. Chance of equal codes is ~1e-6 per attempt.
    int equal = 0;
    const int attempts = 1000;
    for (int i = 0; i < attempts; ++i) {
        const KeyPair vs = gen(), ds = gen(), mitm = gen();
        Outcome vm = run_handshake(Pattern::XX, vs, mitm, nullptr, pr, pr); // vault <-> attacker
        Outcome md = run_handshake(Pattern::XX, mitm, ds, nullptr, pr, pr); // attacker <-> dongle
        CHECK(vm.ok && md.ok);
        if (pairing::sas_code(vm.hv) == pairing::sas_code(md.hd)) ++equal;
    }
    CHECK_MSG(equal <= 1, "%d of %d MITM attempts produced matching codes", equal, attempts);
    // Distribution: 10 buckets over real handshakes, plus a large sample over random hashes.
    int bucket[10] = {};
    const int n = 4000;
    for (int i = 0; i < n; ++i) {
        const KeyPair vs = gen(), ds = gen();
        Outcome o = run_handshake(Pattern::XX, vs, ds, nullptr, pr, pr);
        ++bucket[pairing::sas_code(o.hv) / 100000];
    }
    double chi = 0;
    for (int b = 0; b < 10; ++b) chi += (bucket[b] - n / 10.0) * (bucket[b] - n / 10.0) / (n / 10.0);
    CHECK_MSG(chi < 33.7, "chi-square %.1f over 9 degrees of freedom", chi); // p ~ 1e-4
    int rb[10] = {};
    const int rn = 200000;
    for (int i = 0; i < rn; ++i) {
        uint8_t h[32];
        crypto::random_bytes(h, 32);
        ++rb[pairing::sas_code(h) / 100000];
    }
    double chi2 = 0;
    for (int b = 0; b < 10; ++b) chi2 += (rb[b] - rn / 10.0) * (rb[b] - rn / 10.0) / (rn / 10.0);
    CHECK_MSG(chi2 < 33.7, "chi-square (random h) %.1f", chi2);
    std::printf("  MITM equal codes: %d of %d; chi-square real %.1f, random %.1f (limit 33.7)\n", equal, attempts, chi, chi2);
    // Known answers computed independently (Python hmac): the code itself, not just "both sides agree".
    {
        uint8_t h1[32], h2[32];
        for (int i = 0; i < 32; ++i) h1[i] = static_cast<uint8_t>(i);
        std::memset(h2, 0xAB, 32);
        CHECK(pairing::sas_code(h1) == 367703);
        CHECK(pairing::sas_code(h2) == 539867);
    }
    // Fixed known answer (format and zero padding).
    char t[7];
    pairing::sas_text(42, t);
    CHECK(std::strcmp(t, "000042") == 0);
    pairing::sas_text(999999, t);
    CHECK(std::strcmp(t, "999999") == 0);
}

// The whole bytes-on-the-wire path, including a stream cut at arbitrary points.
static void test_end_to_end_byte_path()
{
    const KeyPair vs = gen(), ds = gen();
    const Bytes pr = prologue();
    Outcome o = run_handshake(Pattern::IK, vs, ds, ds.pk, pr, pr);
    CHECK(o.ok);
    msg::Event ev[3];
    ev[0] = {msg::EventKind::LayoutOn, 0x06, 0, 60, 50, 80};
    ev[1] = {msg::EventKind::Key, 0x02, 0x33, 0, 10, 10};
    ev[2] = {msg::EventKind::LayoutOff, 0x06, 0, 60, 50, 80};
    uint8_t body[1 + 6 * 32], plain[300];
    size_t body_n = 0, plain_n = 0;
    CHECK(msg::encode_type_body(ev, 3, body, sizeof body, &body_n) && body_n == 19);
    CHECK(msg::encode(msg::Type::TypeKeys, 0, 7, body, body_n, plain, sizeof plain, &plain_n));
    uint8_t sealed[400], framed[420];
    size_t sealed_n = 0, framed_n = 0;
    for (size_t chunk : {1u, 2u, 3u, 7u, 20u, 244u}) {
        Outcome s = run_handshake(Pattern::IK, vs, ds, ds.pk, pr, pr);
        CHECK(s.tv.seal(plain, plain_n, sealed, sizeof sealed, &sealed_n) == Status::Ok);
        CHECK(frame::encode(sealed, sealed_n, framed, sizeof framed, &framed_n));
        frame::Reader rd;
        uint8_t got[400];
        size_t got_n = 0;
        size_t off = 0;
        while (off < framed_n && rd.state() == frame::Reader::State::NeedMore) {
            off += rd.feed(framed + off, std::min(chunk, framed_n - off)); // feed() returns bytes CONSUMED
        }
        CHECK(rd.state() == frame::Reader::State::FrameReady && off == framed_n);
        got_n = rd.payload_len();
        std::memcpy(got, rd.payload(), got_n);
        rd.next();
        CHECK_MSG(got_n == sealed_n && std::memcmp(got, sealed, sealed_n) == 0, "chunk %zu", chunk);
        uint8_t opened[300]; size_t opened_n = 0;
        CHECK(s.td.open(got, got_n, opened, sizeof opened, &opened_n) == Status::Ok && opened_n == plain_n);
        msg::Header h; const uint8_t* b = nullptr;
        CHECK(msg::parse(opened, opened_n, &h, &b) == msg::ParseStatus::Ok && h.type == msg::Type::TypeKeys && h.seq == 7 && h.len == body_n);
        msg::Event dec[msg::kMaxEventsPerBatch]; size_t cnt = 0;
        CHECK(msg::decode_type_body(b, h.len, dec, &cnt) == msg::EventsStatus::Ok && cnt == 3);
        CHECK(dec[1].usage == 0x33 && dec[1].mods == 0x02 && dec[2].kind == msg::EventKind::LayoutOff);
    }
}

int main()
{
    CHECK(crypto::selftest() == nullptr);
    test_sizes_and_pairing();
    test_authentication_failures();
    test_every_bit_of_every_handshake_message();
    test_transport_rules();
    test_low_order_points_and_misuse();
    test_sas();
    test_end_to_end_byte_path();
    return finish("protocol");
}
