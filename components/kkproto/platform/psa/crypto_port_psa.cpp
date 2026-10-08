// crypto_port on the PSA Crypto API: what ESP-IDF's mbedTLS provides (and what this project's
// pin_manager already uses). Contains no kkproto logic.
//
// Requires an mbedTLS configuration with X25519 (Curve25519), ChaCha20-Poly1305, SHA-256 and
// HMAC enabled. kk::crypto::selftest() reports at run time if one of them is missing.
#include "kkproto/crypto_port.hpp"

#include "psa/crypto.h"

#include <cstring>

namespace kk::crypto {

namespace {

bool ready()
{
    static const bool ok = (psa_crypto_init() == PSA_SUCCESS); // idempotent; also done at boot by other code
    return ok;
}

void zero(void* p, size_t n)
{
    volatile uint8_t* v = static_cast<volatile uint8_t*>(p);
    while (n-- > 0) *v++ = 0;
}

/// Destroys the key when it goes out of scope, so no early return can leak a key slot.
class Key {
public:
    ~Key()
    {
        if (id_ != PSA_KEY_ID_NULL) psa_destroy_key(id_);
    }
    psa_key_id_t* out() { return &id_; }
    psa_key_id_t id() const { return id_; }

private:
    psa_key_id_t id_ = PSA_KEY_ID_NULL;
};

void make_nonce(uint8_t n[12], uint64_t counter)
{
    std::memset(n, 0, 4);
    for (int i = 0; i < 8; ++i) n[4 + i] = static_cast<uint8_t>(counter >> (8 * i)); // little-endian
}

// RFC 7748 decodeScalar25519. Applied before import so the key is accepted as-is by every PSA
// implementation (the X25519 result is the same with or without it).
void clamp(uint8_t k[kKeyLen])
{
    k[0] &= 248;
    k[31] &= 127;
    k[31] |= 64;
}

bool import_x25519(const uint8_t sk[kKeyLen], Key* key)
{
    uint8_t c[kKeyLen];
    std::memcpy(c, sk, kKeyLen);
    clamp(c);
    psa_key_attributes_t a = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&a, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
    psa_set_key_bits(&a, 255);
    psa_set_key_usage_flags(&a, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&a, PSA_ALG_ECDH);
    const psa_status_t st = psa_import_key(&a, c, kKeyLen, key->out());
    zero(c, sizeof c);
    return st == PSA_SUCCESS;
}

const uint8_t kDummy = 0; // PSA wants a valid pointer even for zero-length inputs

} // namespace

bool x25519_public(const uint8_t sk[kKeyLen], uint8_t pk[kKeyLen])
{
    if (!ready()) return false;
    Key key;
    if (!import_x25519(sk, &key)) return false;
    size_t len = 0;
    return psa_export_public_key(key.id(), pk, kKeyLen, &len) == PSA_SUCCESS && len == kKeyLen;
}

bool x25519_keygen(uint8_t sk[kKeyLen], uint8_t pk[kKeyLen])
{
    if (!ready() || psa_generate_random(sk, kKeyLen) != PSA_SUCCESS) return false;
    return x25519_public(sk, pk);
}

bool x25519(uint8_t out[kKeyLen], const uint8_t sk[kKeyLen], const uint8_t pk[kKeyLen])
{
    if (!ready()) return false;
    Key key;
    if (!import_x25519(sk, &key)) return false;
    size_t len = 0;
    return psa_raw_key_agreement(PSA_ALG_ECDH, key.id(), pk, kKeyLen, out, kKeyLen, &len) == PSA_SUCCESS && len == kKeyLen;
}

void sha256(const uint8_t* a, size_t an, const uint8_t* b, size_t bn, uint8_t out[kHashLen])
{
    uint8_t tmp[kHashLen] = {};
    size_t len = 0;
    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;
    if (ready() && psa_hash_setup(&op, PSA_ALG_SHA_256) == PSA_SUCCESS) {
        if (an > 0) psa_hash_update(&op, a, an);
        if (bn > 0) psa_hash_update(&op, b, bn);
        psa_hash_finish(&op, tmp, sizeof tmp, &len);
    }
    std::memcpy(out, tmp, kHashLen); // out may alias a: written only after the whole digest is done
}

void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* a, size_t an, const uint8_t* b, size_t bn,
                 uint8_t out[kHashLen])
{
    uint8_t tmp[kHashLen] = {};
    size_t len = 0;
    if (ready() && key_len > 0) {
        psa_key_attributes_t at = PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&at, PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&at, key_len * 8);
        psa_set_key_usage_flags(&at, PSA_KEY_USAGE_SIGN_MESSAGE);
        psa_set_key_algorithm(&at, PSA_ALG_HMAC(PSA_ALG_SHA_256));
        Key k;
        if (psa_import_key(&at, key, key_len, k.out()) == PSA_SUCCESS) {
            psa_mac_operation_t op = PSA_MAC_OPERATION_INIT;
            if (psa_mac_sign_setup(&op, k.id(), PSA_ALG_HMAC(PSA_ALG_SHA_256)) == PSA_SUCCESS) {
                if (an > 0) psa_mac_update(&op, a, an);
                if (bn > 0) psa_mac_update(&op, b, bn);
                psa_mac_sign_finish(&op, tmp, sizeof tmp, &len);
            }
        }
    }
    std::memcpy(out, tmp, kHashLen);
}

namespace {
bool import_chacha(const uint8_t key[kKeyLen], psa_key_usage_t usage, Key* k)
{
    psa_key_attributes_t at = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&at, PSA_KEY_TYPE_CHACHA20);
    psa_set_key_bits(&at, 256);
    psa_set_key_usage_flags(&at, usage);
    psa_set_key_algorithm(&at, PSA_ALG_CHACHA20_POLY1305);
    return psa_import_key(&at, key, kKeyLen, k->out()) == PSA_SUCCESS;
}
} // namespace

bool aead_seal(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t* ad, size_t ad_len, const uint8_t* pt,
               size_t pt_len, uint8_t* out)
{
    if (!ready()) return false;
    Key k;
    if (!import_chacha(key, PSA_KEY_USAGE_ENCRYPT, &k)) return false;
    uint8_t n[12];
    make_nonce(n, nonce);
    size_t olen = 0;
    return psa_aead_encrypt(k.id(), PSA_ALG_CHACHA20_POLY1305, n, sizeof n, ad_len ? ad : &kDummy, ad_len,
                            pt_len ? pt : &kDummy, pt_len, out, pt_len + kTagLen, &olen) == PSA_SUCCESS &&
           olen == pt_len + kTagLen;
}

bool aead_open(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t* ad, size_t ad_len, const uint8_t* ct,
               size_t ct_len, uint8_t* pt)
{
    if (!ready() || ct_len < kTagLen) return false;
    Key k;
    if (!import_chacha(key, PSA_KEY_USAGE_DECRYPT, &k)) return false;
    uint8_t n[12];
    make_nonce(n, nonce);
    size_t olen = 0;
    uint8_t scratch = 0;
    const size_t mlen = ct_len - kTagLen;
    return psa_aead_decrypt(k.id(), PSA_ALG_CHACHA20_POLY1305, n, sizeof n, ad_len ? ad : &kDummy, ad_len, ct, ct_len,
                            mlen ? pt : &scratch, mlen ? mlen : 0, &olen) == PSA_SUCCESS &&
           olen == mlen;
}

bool random_bytes(uint8_t* out, size_t n)
{
    return ready() && psa_generate_random(out, n) == PSA_SUCCESS;
}

} // namespace kk::crypto
