#pragma once

#include <cstddef>
#include <cstdint>

namespace kk::crypto {

// =============================================================================
// crypto_port -- the ONLY part of kkproto that differs per platform.
//
// Exactly one implementation is linked into a build:
//   platform/psa/crypto_port_psa.cpp          ESP-IDF (PSA Crypto API)
//   platform/sodium/crypto_port_sodium.cpp    libsodium (nRF52840 / Arduino / host)
//   platform/openssl/crypto_port_openssl.cpp  host reference only (cross-checking)
//
// Everything above this interface (noise, frames, messages, pairing) is plain
// C++17 with no platform dependencies and is shared by the vault and the dongle.
// No exceptions, no heap: callers pass buffers.
// =============================================================================

constexpr size_t kKeyLen = 32;  ///< X25519 keys / shared secrets, ChaCha20-Poly1305 keys
constexpr size_t kHashLen = 32; ///< SHA-256 output
constexpr size_t kTagLen = 16;  ///< Poly1305 tag

/// Fresh X25519 key pair from the hardware / OS random source.
bool x25519_keygen(uint8_t sk[kKeyLen], uint8_t pk[kKeyLen]);

/// Public key for a private key (RFC 7748 clamping is applied internally).
bool x25519_public(const uint8_t sk[kKeyLen], uint8_t pk[kKeyLen]);

/// X25519(sk, pk). false on failure. noise.cpp additionally rejects an
/// all-zero result itself, so ports need not (but may) do so.
bool x25519(uint8_t out[kKeyLen], const uint8_t sk[kKeyLen], const uint8_t pk[kKeyLen]);

/// SHA-256 over a || b (b may be nullptr with bn == 0). out MAY alias a: noise.cpp
/// relies on that for h = HASH(h || data).
void sha256(const uint8_t* a, size_t an, const uint8_t* b, size_t bn, uint8_t out[kHashLen]);

/// HMAC-SHA256 over a || b (b may be nullptr with bn == 0). Any key length.
void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* a, size_t an, const uint8_t* b, size_t bn,
                 uint8_t out[kHashLen]);

/// ChaCha20-Poly1305 (RFC 8439). The 96-bit nonce is the Noise one: 4 zero
/// bytes followed by |nonce| as a 64-bit little-endian counter.
/// out receives pt_len + kTagLen bytes (ciphertext followed by the tag).
bool aead_seal(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t* ad, size_t ad_len, const uint8_t* pt,
               size_t pt_len, uint8_t* out);

/// Inverse of aead_seal(); ct_len includes the tag; pt receives ct_len - kTagLen
/// bytes. false if the tag does not verify (pt is then unspecified).
bool aead_open(const uint8_t key[kKeyLen], uint64_t nonce, const uint8_t* ad, size_t ad_len, const uint8_t* ct,
               size_t ct_len, uint8_t* pt);

/// Cryptographically secure random bytes.
bool random_bytes(uint8_t* out, size_t n);

/// Known-answer self-test of the linked port (SHA-256, HMAC-SHA256, X25519,
/// ChaCha20-Poly1305, RNG). Returns nullptr if everything passed, otherwise a
/// short name of the first failing primitive. Cheap enough to run at boot:
/// it is how a device finds out that an SDK configuration left an algorithm out.
const char* selftest();

} // namespace kk::crypto
