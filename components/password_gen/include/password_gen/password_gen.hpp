#pragma once

#include "settings/settings_types.hpp"

#include <cstddef>

// =============================================================================
// password_gen -- random password generation.
//
// Redesigned from KeyKeeper 1.90's own generator, not a copy -- see
// this component's README for the full comparison. Two real
// differences from 1.90:
//
//   1. Character CLASSES (uppercase/lowercase/digits/symbols), each a
//      plain on/off toggle, instead of 1.90's single free-text
//      "allowed characters" string (passchars). On THIS device's
//      encoder input model, four togglable booleans are fast to set;
//      typing a custom character set one character-wheel spin at a
//      time (the only way to enter free text here, see
//      widgets::TextEntry) would rarely get used in practice. If a
//      selected length can't fit one of every enabled class, this
//      still guarantees as many classes as fit (see generate()'s own
//      comment) -- 1.90's flat string had no such guarantee at all,
//      since it never distinguished classes to begin with.
//
//   2. esp_random() (this chip's hardware TRNG) instead of 1.90's
//      plain, non-cryptographic random(). Same hardware RNG this
//      project already relies on elsewhere (security::pin's salt
//      generation uses esp_fill_random(), built on the same
//      generator) -- not a new trust dependency, just used here too.
//      Sampled via rejection sampling against the exact class/pool
//      size (see password_gen.cpp's uniform_random()), not a plain
//      modulo, to avoid the small-but-real bias that would introduce.
// =============================================================================

namespace password_gen {

inline constexpr uint8_t MIN_LENGTH = 4;
inline constexpr uint8_t MAX_LENGTH = 64;

/**
 * @brief Generate a random password per cfg into out (null-terminated).
 *
 * @param out_capacity Must be at least cfg.length + 1.
 * @return false if cfg.length is outside [MIN_LENGTH, MAX_LENGTH],
 *         out_capacity is too small, or no character class is
 *         enabled at all (nothing to draw from) -- out is left
 *         untouched in every failure case.
 */
bool generate(const settings::PasswordGenSettings& cfg, char* out, size_t out_capacity);

} // namespace password_gen
