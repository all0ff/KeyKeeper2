// Checks that need a deliberately BROKEN crypto_port, to prove two defences work:
//   -DLEAKY   noise.cpp rejects an all-zero DH result even if the port hands one back
//   -DNOAUTH  selftest() notices a port whose AEAD does not authenticate
#include "kkproto/noise.hpp"
#include "kkproto/pairing.hpp"
#include "test_support.hpp"

#include <cstring>

using namespace kk;
using namespace kk::noise;

int main()
{
#ifdef LEAKY
    const char* low[] = {"0000000000000000000000000000000000000000000000000000000000000000",
                         "0100000000000000000000000000000000000000000000000000000000000000",
                         "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
                         "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157"};
    KeyPair ds{};
    crypto::x25519_keygen(ds.sk, ds.pk);
    uint8_t pro[pairing::kPrologueLen];
    pairing::make_prologue(pro);
    for (const char* hex : low) {
        const Bytes e = from_hex(hex);
        uint8_t raw[32];
        CHECK(crypto::x25519(raw, ds.sk, e.data()));                // precondition: THIS port reports success...
        uint8_t zero[32] = {};
        CHECK(std::memcmp(raw, zero, 32) == 0);                      // ...with an all-zero result
        HandshakeState d;
        HandshakeState::Config c;
        c.pattern = Pattern::XX; c.role = Role::Responder; c.prologue = pro; c.prologue_len = sizeof pro; c.s = &ds;
        CHECK(d.init(c) == Status::Ok);
        uint8_t pl[8], out[200];
        size_t pn = 0, on = 0;
        CHECK(d.read_message(e.data(), e.size(), pl, sizeof pl, &pn) == Status::Ok);
        CHECK_MSG(d.write_message(nullptr, 0, out, sizeof out, &on) == Status::BadDhResult, "core accepted a zero DH result");
    }
    return finish("core zero-DH check (leaky port)");
#elif defined(NOAUTH)
    uint8_t key[32] = {1}, ct[32], back[32];
    const uint8_t pt[] = {1, 2, 3, 4};
    CHECK(crypto::aead_seal(key, 5, nullptr, 0, pt, 4, ct));
    ct[1] ^= 1;
    CHECK(crypto::aead_open(key, 5, nullptr, 0, ct, 4 + 16, back));  // precondition: this port accepts a forged message
    const char* r = crypto::selftest();
    CHECK_MSG(r != nullptr, "selftest passed a port that does not authenticate");
    std::printf("  selftest says: %s\n", r ? r : "(nothing)");
    return finish("selftest vs non-authenticating port");
#else
#error "build with -DLEAKY or -DNOAUTH"
#endif
}
