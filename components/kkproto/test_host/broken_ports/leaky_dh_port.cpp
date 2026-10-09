// TEST-ONLY port: libsodium, except x25519() reports SUCCESS with an all-zero result for
// low-order public keys, the way a port without that check would. Proves that noise.cpp's own
// all-zero check works even when the platform library does not reject such keys.
#define x25519 x25519_strict
#include "../../platform/sodium/crypto_port_sodium.cpp"
#undef x25519

namespace kk::crypto {
bool x25519(uint8_t out[kKeyLen], const uint8_t sk[kKeyLen], const uint8_t pk[kKeyLen])
{
    if (x25519_strict(out, sk, pk)) return true;
    std::memset(out, 0, kKeyLen);
    return true;
}
} // namespace kk::crypto
