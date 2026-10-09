// Deterministic scenario printed as hex. Built once per crypto_port; the outputs of all ports
// must be byte-identical (different libraries, same bytes on the wire).
#include "kkproto/noise.hpp"
#include "kkproto/pairing.hpp"
#include "test_support.hpp"

#include <cstring>

using namespace kk;
using namespace kk::noise;

static KeyPair fixed(uint8_t seed)
{
    KeyPair k{};
    for (int i = 0; i < 32; ++i) k.sk[i] = static_cast<uint8_t>(seed * 31 + i * 7 + 3);
    crypto::x25519_public(k.sk, k.pk);
    return k;
}

static void line(const char* name, const uint8_t* p, size_t n) { std::printf("%s %s\n", name, to_hex(p, n).c_str()); }

int main()
{
    const char* st = crypto::selftest();
    std::printf("selftest %s\n", st ? st : "OK");
    const KeyPair vs = fixed(1), ds = fixed(2), ve = fixed(3), de = fixed(4), ve2 = fixed(5), de2 = fixed(6);
    uint8_t pro[pairing::kPrologueLen];
    pairing::make_prologue(pro);
    line("vault_static_pk", vs.pk, 32);
    line("dongle_static_pk", ds.pk, 32);

    // pairing: XX
    HandshakeState v, d;
    HandshakeState::Config cv, cd;
    cv.pattern = cd.pattern = Pattern::XX;
    cv.role = Role::Initiator; cd.role = Role::Responder;
    cv.prologue = cd.prologue = pro; cv.prologue_len = cd.prologue_len = sizeof pro;
    cv.s = &vs; cd.s = &ds; cv.test_e = &ve; cd.test_e = &de;
    v.init(cv); d.init(cd);
    const uint8_t hello[] = {1, 0}, ack[] = {1, 1, 0, 1};
    uint8_t w[256], r[256];
    size_t wn = 0, rn = 0;
    v.write_message(nullptr, 0, w, sizeof w, &wn);           line("xx1", w, wn);  d.read_message(w, wn, r, sizeof r, &rn);
    d.write_message(ack, sizeof ack, w, sizeof w, &wn);      line("xx2", w, wn);  v.read_message(w, wn, r, sizeof r, &rn);
    v.write_message(hello, sizeof hello, w, sizeof w, &wn);  line("xx3", w, wn);  d.read_message(w, wn, r, sizeof r, &rn);
    line("xx_hash", v.handshake_hash(), 32);
    char sas[7];
    pairing::sas_text(pairing::sas_code(v.handshake_hash()), sas);
    std::printf("xx_sas %s\n", sas);

    // session: IK
    HandshakeState v2, d2;
    HandshakeState::Config c2v, c2d;
    c2v.pattern = c2d.pattern = Pattern::IK;
    c2v.role = Role::Initiator; c2d.role = Role::Responder;
    c2v.prologue = c2d.prologue = pro; c2v.prologue_len = c2d.prologue_len = sizeof pro;
    c2v.s = &vs; c2d.s = &ds; c2v.rs = ds.pk; c2v.test_e = &ve2; c2d.test_e = &de2;
    v2.init(c2v); d2.init(c2d);
    v2.write_message(nullptr, 0, w, sizeof w, &wn);          line("ik1", w, wn);  d2.read_message(w, wn, r, sizeof r, &rn);
    d2.write_message(nullptr, 0, w, sizeof w, &wn);          line("ik2", w, wn);  v2.read_message(w, wn, r, sizeof r, &rn);
    line("ik_hash", v2.handshake_hash(), 32);
    Transport tv, td;
    v2.split(&tv); d2.split(&td);
    const uint8_t m1[] = {'T', 'Y', 'P', 'E'}, m2[] = {'a', 'b', 'c', 'd', 'e', 'f'}, m3[] = {'O', 'K'};
    tv.seal(m1, sizeof m1, w, sizeof w, &wn);   line("t_v2d_1", w, wn);  td.open(w, wn, r, sizeof r, &rn);
    tv.seal(m2, sizeof m2, w, sizeof w, &wn);   line("t_v2d_2", w, wn);  td.open(w, wn, r, sizeof r, &rn);
    td.seal(m3, sizeof m3, w, sizeof w, &wn);   line("t_d2v_1", w, wn);  tv.open(w, wn, r, sizeof r, &rn);
    return 0;
}
