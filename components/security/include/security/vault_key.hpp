#pragma once

#include "psa/crypto.h"

#include <cstddef>
#include <cstdint>

// =============================================================================
// security::vault_key -- holds this session's vault.db encryption key
// (AES-256, for vault::repository's own AEAD encrypt/decrypt calls)
// in memory for as long as the device stays Unlocked. Deliberately a
// separate, narrow module from pin_manager/lock_manager rather than
// exposing this from either of THEM directly -- security:: must not
// depend on vault:: (see vault.hpp's own file comment on the
// dependency direction; lock_manager.cpp's own comment on the same
// rule, re: who's allowed to actually wipe the vault, is the same
// reasoning), so the key has to live on the security:: side of that
// boundary for vault:: to reach across and read it, not the other
// way around.
//
// WHY THIS EXISTS AT ALL, not just re-deriving the key from the PIN
// whenever vault:: needs it: pin_manager::verify()'s own PBKDF2
// result (candidate_hash) is zeroed before it ever returns to its
// caller (a deliberate hygiene choice, unrelated to this feature) --
// so the ONLY place the key material is ever available at all is
// inside verify()/verify_duress() themselves, for the few lines
// between computing it and zeroing it. set() is called from exactly
// there (see pin_manager.cpp), nowhere else -- this module itself
// never computes or re-derives anything, it only holds what it's
// given until clear() is called.
//
// The actual 32 raw key bytes are NOT kept around after set() --
// immediately imported as a PSA key (psa_import_key()) and only the
// resulting opaque psa_key_id_t handle is retained. vault::
// repository's own AEAD calls (psa_aead_encrypt()/psa_aead_decrypt())
// take this handle directly, never the raw bytes.
// =============================================================================

namespace security::vault_key {

/**
 * @brief Import a fresh 32-byte AES-256 key for this session,
 *        destroying whatever key (if any) was set before.
 *
 * Called only from pin_manager.cpp's verify()/verify_duress(), on a
 * successful PIN check, with the derived (not raw-PBKDF2) key -- see
 * that derivation's own comment there for why it's a separate value
 * from the PIN's own verification hash.
 *
 * @param key Exactly 32 bytes. This function does not modify or clear
 *            the caller's copy -- the caller (pin_manager.cpp) is
 *            responsible for zeroing its own local buffer right after
 *            this returns, matching how it already handles
 *            candidate_hash.
 * @return false if the PSA import itself failed -- treat the same as
 *         "no key available" (is_set() stays false).
 */
bool set(const uint8_t key[32]);

bool is_set();

/**
 * @brief The current session's PSA key handle, for vault::repository
 *        to pass directly to psa_aead_encrypt()/psa_aead_decrypt().
 *
 * @return 0 (an invalid psa_key_id_t) if is_set() is false.
 */
psa_key_id_t handle();

/// Destroys the PSA key (if any) and resets to "no key". Called from
/// lock_manager.cpp's lock() and wipe paths -- the device being
/// Locked (or the vault being wiped) must never leave a usable vault
/// key sitting in memory.
void clear();

} // namespace security::vault_key
