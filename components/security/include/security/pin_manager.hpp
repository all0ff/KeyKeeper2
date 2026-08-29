#pragma once

#include <cstdint>

// =============================================================================
// security::pin -- PinManager
//
// Per docs/ARCHITECTURE.md's SecurityService breakdown: stores PIN
// parameters, verifies/changes the PIN, tracks wrong attempts, and
// enforces a lockout after too many consecutive failures.
//
// SCOPE BOUNDARY (per the user's explicit decision -- see
// components/security/README.md): this is NOT the future
// AES-256/key-derivation layer. The PIN is never used to derive an
// encryption key, and nothing here encrypts vault.db. PIN verification
// only gates logical/software access to the firmware's own UI/API --
// a flash dump still exposes vault.db in plaintext. That limitation
// is intentional for this version (REQUIREMENTS.md 9.4: encryption is
// "Future Security"), not an oversight.
//
// One design choice made here that sits right on that boundary and is
// called out explicitly rather than assumed: the PIN is stored as a
// salted SHA-256 digest (via mbedtls), not in plaintext. This is a
// single, unstretched hash used ONLY to verify a guess against the
// stored digest -- it is NOT key derivation (no KDF stretching, the
// output is never used as an encryption key, nothing downstream
// depends on it being slow to compute). The intent is "don't leave
// the PIN sitting in NVS as cleartext", not "start building the future
// crypto layer early". If that reasoning doesn't hold up, this is a
// small, isolated piece to revisit.
// =============================================================================

namespace security::pin {

enum class VerifyResult : uint8_t
{
    Success,
    WrongPin,
    LockedOut,
    NoPinSet,
};

/**
 * @brief Load PIN parameters from NVS (via storage::nvs), if any were
 *        previously set.
 *
 * Must be called after storage::init() and settings::init(). Safe to
 * call once; a second call is a no-op that returns true. Does NOT
 * require a PIN to already be set -- has_pin() reports whether one is
 * (first boot: no PIN, device effectively "open" until one is set;
 * that first-setup UX is a GUI/system concern, not this component's).
 */
bool init();

bool is_initialized();

bool has_pin();

/**
 * @brief Set or replace the PIN.
 *
 * @param new_pin Digits only, length checked against
 *                settings::all().security.pin_length (4-6 per
 *                REQUIREMENTS 12.3).
 * @param old_pin Required and verified if has_pin() is true. Pass
 *                nullptr only for first-time setup (!has_pin()).
 *
 * Publishes event_bus::SystemEventId::PinChanged on success.
 *
 * @return true on success.
 */
bool set_pin(const char* new_pin, const char* old_pin);

/**
 * @brief Verify a PIN guess.
 *
 * Consecutive wrong guesses count toward a lockout (see
 * components/security/README.md for the current threshold/duration --
 * placeholder values, not a considered anti-bruteforce policy). A
 * correct guess resets the counter.
 */
VerifyResult verify(const char* pin);

/// Attempts left before lockout kicks in. 0 while locked out.
uint8_t attempts_remaining();

bool is_locked_out();

} // namespace security::pin
