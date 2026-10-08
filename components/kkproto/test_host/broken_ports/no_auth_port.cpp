// TEST-ONLY port: libsodium, except aead_open() decrypts correctly but NEVER checks the
// Poly1305 tag (a realistic porting bug). selftest() must catch it.
#define aead_open aead_open_strict
#include "../../platform/sodium/crypto_port_sodium.cpp"
#undef aead_open

namespace kk::crypto {
bool aead_open(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t*, size_t, const uint8_t* ct, size_t ct_len,
               uint8_t* pt)
{
    if (ct_len < kTagLen) return false;
    uint8_t n[12];
    std::memset(n, 0, 4);
    for (int i = 0; i < 8; ++i) n[4 + i] = static_cast<uint8_t>(nonce >> (8 * i));
    crypto_stream_chacha20_ietf_xor_ic(pt, ct, ct_len - kTagLen, n, 1, key); // counter 1: block 0 is the Poly1305 key
    return true;
}
} // namespace kk::crypto
