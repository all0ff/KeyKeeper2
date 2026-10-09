// crypto_port on OpenSSL 3. HOST REFERENCE ONLY: used to cross-check the other ports.
#include "kkproto/crypto_port.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <memory>

namespace kk::crypto {

namespace {

struct PkeyDeleter { void operator()(EVP_PKEY* p) const { EVP_PKEY_free(p); } };
using PkeyPtr = std::unique_ptr<EVP_PKEY, PkeyDeleter>;

void make_nonce(uint8_t n[12], uint64_t counter)
{
    std::memset(n, 0, 4);
    for (int i = 0; i < 8; ++i) {
        n[4 + i] = static_cast<uint8_t>(counter >> (8 * i));
    }
}

} // namespace

bool x25519_public(const uint8_t sk[kKeyLen], uint8_t pk[kKeyLen])
{
    EVP_PKEY* k = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, sk, kKeyLen);
    if (k == nullptr) return false;
    size_t len = kKeyLen;
    const bool ok = EVP_PKEY_get_raw_public_key(k, pk, &len) == 1 && len == kKeyLen;
    EVP_PKEY_free(k);
    return ok;
}

bool x25519_keygen(uint8_t sk[kKeyLen], uint8_t pk[kKeyLen])
{
    return RAND_bytes(sk, kKeyLen) == 1 && x25519_public(sk, pk);
}

bool x25519(uint8_t out[kKeyLen], const uint8_t sk[kKeyLen], const uint8_t pk[kKeyLen])
{
    EVP_PKEY* priv = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, sk, kKeyLen);
    EVP_PKEY* pub = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, pk, kKeyLen);
    bool ok = false;
    if (priv != nullptr && pub != nullptr) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(priv, nullptr);
        size_t len = kKeyLen;
        ok = ctx != nullptr && EVP_PKEY_derive_init(ctx) == 1 && EVP_PKEY_derive_set_peer(ctx, pub) == 1 &&
             EVP_PKEY_derive(ctx, out, &len) == 1 && len == kKeyLen;
        EVP_PKEY_CTX_free(ctx);
    }
    EVP_PKEY_free(priv);
    EVP_PKEY_free(pub);
    return ok;
}

void sha256(const uint8_t* a, size_t an, const uint8_t* b, size_t bn, uint8_t out[kHashLen])
{
    EVP_MD_CTX* c = EVP_MD_CTX_new();
    uint8_t tmp[kHashLen];
    unsigned len = 0;
    EVP_DigestInit_ex(c, EVP_sha256(), nullptr);
    if (an > 0) EVP_DigestUpdate(c, a, an);
    if (bn > 0) EVP_DigestUpdate(c, b, bn);
    EVP_DigestFinal_ex(c, tmp, &len);
    EVP_MD_CTX_free(c);
    std::memcpy(out, tmp, kHashLen);
}

void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* a, size_t an, const uint8_t* b, size_t bn,
                 uint8_t out[kHashLen])
{
    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    EVP_MAC_CTX* c = EVP_MAC_CTX_new(mac);
    char digest[] = "SHA256";
    OSSL_PARAM params[2] = {OSSL_PARAM_construct_utf8_string("digest", digest, 0), OSSL_PARAM_construct_end()};
    uint8_t tmp[kHashLen];
    size_t len = 0;
    const uint8_t dummy = 0;
    EVP_MAC_init(c, key_len > 0 ? key : &dummy, key_len, params);
    if (an > 0) EVP_MAC_update(c, a, an);
    if (bn > 0) EVP_MAC_update(c, b, bn);
    EVP_MAC_final(c, tmp, &len, sizeof tmp);
    EVP_MAC_CTX_free(c);
    EVP_MAC_free(mac);
    std::memcpy(out, tmp, kHashLen);
}

bool aead_seal(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t* ad, size_t ad_len, const uint8_t* pt,
               size_t pt_len, uint8_t* out)
{
    uint8_t iv[12];
    make_nonce(iv, nonce);
    EVP_CIPHER_CTX* c = EVP_CIPHER_CTX_new();
    int n = 0, total = 0;
    bool ok = EVP_EncryptInit_ex(c, EVP_chacha20_poly1305(), nullptr, nullptr, nullptr) == 1 &&
              EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr) == 1 &&
              EVP_EncryptInit_ex(c, nullptr, nullptr, key, iv) == 1;
    if (ok && ad_len > 0) ok = EVP_EncryptUpdate(c, nullptr, &n, ad, static_cast<int>(ad_len)) == 1;
    if (ok && pt_len > 0) {
        ok = EVP_EncryptUpdate(c, out, &n, pt, static_cast<int>(pt_len)) == 1;
        total = n;
    }
    if (ok) {
        uint8_t fin[16];
        ok = EVP_EncryptFinal_ex(c, fin, &n) == 1 && EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_AEAD_GET_TAG, 16, out + pt_len) == 1;
        (void)total;
    }
    EVP_CIPHER_CTX_free(c);
    return ok;
}

bool aead_open(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t* ad, size_t ad_len, const uint8_t* ct,
               size_t ct_len, uint8_t* pt)
{
    if (ct_len < kTagLen) return false;
    const size_t mlen = ct_len - kTagLen;
    uint8_t iv[12];
    make_nonce(iv, nonce);
    uint8_t tag[16];
    std::memcpy(tag, ct + mlen, 16);
    EVP_CIPHER_CTX* c = EVP_CIPHER_CTX_new();
    int n = 0;
    bool ok = EVP_DecryptInit_ex(c, EVP_chacha20_poly1305(), nullptr, nullptr, nullptr) == 1 &&
              EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr) == 1 &&
              EVP_DecryptInit_ex(c, nullptr, nullptr, key, iv) == 1;
    if (ok && ad_len > 0) ok = EVP_DecryptUpdate(c, nullptr, &n, ad, static_cast<int>(ad_len)) == 1;
    if (ok && mlen > 0) ok = EVP_DecryptUpdate(c, pt, &n, ct, static_cast<int>(mlen)) == 1;
    if (ok) ok = EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_AEAD_SET_TAG, 16, tag) == 1;
    if (ok) {
        uint8_t fin[16];
        ok = EVP_DecryptFinal_ex(c, fin, &n) == 1; // verifies the tag
    }
    EVP_CIPHER_CTX_free(c);
    return ok;
}

bool random_bytes(uint8_t* out, size_t n)
{
    return RAND_bytes(out, static_cast<int>(n)) == 1;
}

} // namespace kk::crypto
