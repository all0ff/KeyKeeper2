// crypto_port on libsodium. Intended for the nRF52840 dongle (Arduino / nRF Connect SDK
// with libsodium) and for host builds. Contains no kkproto logic.
#include "kkproto/crypto_port.hpp"

#include <sodium.h>

#include <cstring>

namespace kk::crypto {

namespace {
bool ready()
{
    static const bool ok = (sodium_init() >= 0);
    return ok;
}
void make_nonce(uint8_t n[crypto_aead_chacha20poly1305_ietf_NPUBBYTES], uint64_t counter)
{
    std::memset(n, 0, 4);
    for (int i = 0; i < 8; ++i) {
        n[4 + i] = static_cast<uint8_t>(counter >> (8 * i)); // little-endian
    }
}
} // namespace

bool x25519_keygen(uint8_t sk[kKeyLen], uint8_t pk[kKeyLen])
{
    if (!ready()) return false;
    randombytes_buf(sk, kKeyLen);
    return crypto_scalarmult_curve25519_base(pk, sk) == 0;
}

bool x25519_public(const uint8_t sk[kKeyLen], uint8_t pk[kKeyLen])
{
    return ready() && crypto_scalarmult_curve25519_base(pk, sk) == 0;
}

bool x25519(uint8_t out[kKeyLen], const uint8_t sk[kKeyLen], const uint8_t pk[kKeyLen])
{
    return ready() && crypto_scalarmult_curve25519(out, sk, pk) == 0; // -1 for an all-zero result
}

void sha256(const uint8_t* a, size_t an, const uint8_t* b, size_t bn, uint8_t out[kHashLen])
{
    crypto_hash_sha256_state st;
    crypto_hash_sha256_init(&st);
    if (an > 0) crypto_hash_sha256_update(&st, a, an);
    if (bn > 0) crypto_hash_sha256_update(&st, b, bn);
    crypto_hash_sha256_final(&st, out); // out is written only here, so it may alias a
}

void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* a, size_t an, const uint8_t* b, size_t bn,
                 uint8_t out[kHashLen])
{
    crypto_auth_hmacsha256_state st;
    crypto_auth_hmacsha256_init(&st, key, key_len);
    if (an > 0) crypto_auth_hmacsha256_update(&st, a, an);
    if (bn > 0) crypto_auth_hmacsha256_update(&st, b, bn);
    crypto_auth_hmacsha256_final(&st, out);
}

bool aead_seal(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t* ad, size_t ad_len, const uint8_t* pt,
               size_t pt_len, uint8_t* out)
{
    if (!ready()) return false;
    uint8_t n[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
    make_nonce(n, nonce);
    unsigned long long clen = 0;
    return crypto_aead_chacha20poly1305_ietf_encrypt(out, &clen, pt, pt_len, ad, ad_len, nullptr, n, key) == 0 &&
           clen == pt_len + kTagLen;
}

bool aead_open(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t* ad, size_t ad_len, const uint8_t* ct,
               size_t ct_len, uint8_t* pt)
{
    if (!ready() || ct_len < kTagLen) return false;
    uint8_t n[crypto_aead_chacha20poly1305_ietf_NPUBBYTES];
    make_nonce(n, nonce);
    unsigned long long mlen = 0;
    return crypto_aead_chacha20poly1305_ietf_decrypt(pt, &mlen, nullptr, ct, ct_len, ad, ad_len, n, key) == 0;
}

bool random_bytes(uint8_t* out, size_t n)
{
    if (!ready()) return false;
    randombytes_buf(out, n);
    return true;
}

} // namespace kk::crypto
