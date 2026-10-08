#include "kkproto/crypto_port.hpp"

#include <cstring>

namespace kk::crypto {

namespace {

bool hex_to_bytes(const char* hex, uint8_t* out, size_t n)
{
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    for (size_t i = 0; i < n; ++i) {
        const int hi = nib(hex[2 * i]), lo = nib(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

bool eq_hex(const uint8_t* got, const char* hex, size_t n)
{
    uint8_t want[64];
    return n <= sizeof want && hex_to_bytes(hex, want, n) && std::memcmp(got, want, n) == 0;
}

} // namespace

const char* selftest()
{
    // SHA-256("abc") -- FIPS 180-4 example; also exercises the two-part form and out == a aliasing.
    {
        const uint8_t abc[] = {'a', 'b', 'c'};
        uint8_t h[kHashLen];
        sha256(abc, 3, nullptr, 0, h);
        if (!eq_hex(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 32)) return "sha256";
        uint8_t buf[kHashLen + 1];
        sha256(abc, 1, abc + 1, 2, h); // "a" || "bc"
        if (!eq_hex(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 32)) return "sha256 (two-part)";
        std::memset(buf, 0, sizeof buf);
        std::memcpy(buf, h, kHashLen);
        buf[kHashLen] = 0x01;
        sha256(buf, kHashLen, buf + kHashLen, 1, buf); // out aliases a
        uint8_t want[kHashLen];
        sha256(h, kHashLen, buf + kHashLen, 1, want);
        if (std::memcmp(buf, want, kHashLen) != 0) return "sha256 (aliased output)";
    }
    // HMAC-SHA256 -- RFC 4231 test case 1, plus the two-part form.
    {
        uint8_t key[20];
        std::memset(key, 0x0b, sizeof key);
        const uint8_t hi[] = {'H', 'i', ' ', 'T', 'h', 'e', 'r', 'e'};
        uint8_t mac[kHashLen];
        hmac_sha256(key, sizeof key, hi, sizeof hi, nullptr, 0, mac);
        if (!eq_hex(mac, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", 32)) return "hmac-sha256";
        hmac_sha256(key, sizeof key, hi, 3, hi + 3, 5, mac);
        if (!eq_hex(mac, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", 32)) return "hmac-sha256 (two-part)";
    }
    // X25519 -- RFC 7748 section 6.1 (Alice and Bob).
    {
        uint8_t ask[32], bsk[32], apk[32], bpk[32], s1[32], s2[32];
        if (!hex_to_bytes("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", ask, 32)) return "selftest data";
        if (!hex_to_bytes("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb", bsk, 32)) return "selftest data";
        if (!x25519_public(ask, apk) || !eq_hex(apk, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a", 32)) return "x25519 public";
        if (!x25519_public(bsk, bpk) || !eq_hex(bpk, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f", 32)) return "x25519 public";
        if (!x25519(s1, ask, bpk) || !x25519(s2, bsk, apk)) return "x25519";
        if (!eq_hex(s1, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742", 32) ||
            std::memcmp(s1, s2, 32) != 0) {
            return "x25519 shared secret";
        }
    }
    // ChaCha20-Poly1305 with the Noise nonce layout. Value computed with both libsodium and
    // OpenSSL (identical); the primitive itself is RFC 8439.
    {
        uint8_t key[32];
        for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i);
        const uint8_t ad[] = {'k', 'k', '2', '-', 's', 'e', 'l', 'f', 't', 'e', 's', 't'};
        const uint8_t pt[] = {'K', 'e', 'y', 'K', 'e', 'e', 'p', 'e', 'r', '2'};
        const uint64_t nonce = 0x0102030405060708ull;
        uint8_t ct[sizeof pt + kTagLen];
        if (!aead_seal(key, nonce, ad, sizeof ad, pt, sizeof pt, ct)) return "aead seal";
        if (!eq_hex(ct, "bb2cd4afce5c9a7d169997659b808b8de5bc2e2c682754b0ef07", sizeof ct)) return "aead known answer";
        uint8_t back[sizeof pt];
        if (!aead_open(key, nonce, ad, sizeof ad, ct, sizeof ct, back) || std::memcmp(back, pt, sizeof pt) != 0) return "aead open";
        ct[3] ^= 0x01;
        if (aead_open(key, nonce, ad, sizeof ad, ct, sizeof ct, back)) return "aead accepts a forged message";
        ct[3] ^= 0x01;
        if (aead_open(key, nonce + 1, ad, sizeof ad, ct, sizeof ct, back)) return "aead accepts a wrong nonce";
        // empty plaintext and empty AD (the handshake uses both)
        uint8_t tag[kTagLen];
        if (!aead_seal(key, 0, nullptr, 0, nullptr, 0, tag) || !aead_open(key, 0, nullptr, 0, tag, kTagLen, back)) return "aead empty";
    }
    // Random source: not all zero, two draws differ.
    {
        uint8_t a[32] = {}, b[32] = {};
        if (!random_bytes(a, 32) || !random_bytes(b, 32)) return "rng";
        uint8_t acc = 0;
        for (int i = 0; i < 32; ++i) acc = static_cast<uint8_t>(acc | a[i]);
        if (acc == 0 || std::memcmp(a, b, 32) == 0) return "rng output";
    }
    return nullptr;
}

} // namespace kk::crypto
