#pragma once

#include <cstddef>
#include <cstdint>

namespace kk::pairing {

/// Noise prologue: the text below followed by the protocol version byte. Both
/// sides must use exactly this, or the handshake fails (it is hashed into h).
constexpr char kPrologueText[] = "KeyKeeper2-link-v1";
constexpr uint8_t kPrologueVersion = 1;
constexpr size_t kPrologueLen = sizeof(kPrologueText) - 1 + 1;

void make_prologue(uint8_t out[kPrologueLen]);

/// Verification code shown on the vault and typed by the dongle: the first 4
/// bytes (big-endian) of HMAC-SHA256(key = h, message = "kk2-sas") modulo 10^6,
/// where h is the final Noise handshake hash. h covers both static and both
/// ephemeral keys, so a man in the middle yields different codes on the two ends.
uint32_t sas_code(const uint8_t handshake_hash[32]);

/// 6 decimal digits (zero-padded) and a terminating NUL.
void sas_text(uint32_t code, char out[7]);

} // namespace kk::pairing
