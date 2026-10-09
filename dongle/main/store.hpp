#pragma once

#include <cstdint>

// Persistent storage for the link: this device's long-term secret key and the one peer it is paired with.
// NVS, namespace "kklink". The secret key is stored in the clear for now (flash encryption comes with the
// production build); the public key is always derived from it, never stored.

namespace dongle::store {

bool init();

bool load_secret(uint8_t sk[32]); ///< false if there is none (or it has the wrong size)
bool save_secret(const uint8_t sk[32]);

bool load_peer(uint8_t pk[32]);
bool save_peer(const uint8_t pk[32]);
bool erase_peer();

} // namespace dongle::store
