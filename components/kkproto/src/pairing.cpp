#include "kkproto/pairing.hpp"

#include "kkproto/crypto_port.hpp"

#include <cstring>

namespace kk::pairing {

void make_prologue(uint8_t out[kPrologueLen])
{
    std::memcpy(out, kPrologueText, kPrologueLen - 1);
    out[kPrologueLen - 1] = kPrologueVersion;
}

uint32_t sas_code(const uint8_t handshake_hash[32])
{
    static const uint8_t kMsg[] = {'k', 'k', '2', '-', 's', 'a', 's'};
    uint8_t mac[crypto::kHashLen];
    crypto::hmac_sha256(handshake_hash, 32, kMsg, sizeof kMsg, nullptr, 0, mac);
    const uint32_t v = (static_cast<uint32_t>(mac[0]) << 24) | (static_cast<uint32_t>(mac[1]) << 16) |
                       (static_cast<uint32_t>(mac[2]) << 8) | mac[3];
    return v % 1000000u;
}

void sas_text(uint32_t code, char out[7])
{
    for (int i = 5; i >= 0; --i) {
        out[i] = static_cast<char>('0' + code % 10);
        code /= 10;
    }
    out[6] = '\0';
}

} // namespace kk::pairing
