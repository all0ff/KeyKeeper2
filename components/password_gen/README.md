# components/password_gen

Random password generation, redesigned from KeyKeeper 1.90's own
generator rather than copied -- see `password_gen.hpp`'s file comment
for the full reasoning. Short version:

- **Four character-class toggles** (uppercase/lowercase/digits/symbols),
  each a plain on/off setting, instead of 1.90's single free-text
  "allowed characters" string. Typing a custom character set via this
  device's character-wheel `widgets::TextEntry` (the only way to enter
  free text here) would be slow and rarely used in practice; four
  togglable booleans fit the encoder input model far better.
- **`esp_random()`** (hardware TRNG) instead of 1.90's plain
  `random()`. Not a new trust dependency -- `components/security`
  already relies on the same generator (`esp_fill_random()`) for PIN
  salt generation.
- Uniform sampling via rejection sampling against the exact
  class/pool size, not a plain modulo -- avoids the small bias a
  charset size that doesn't evenly divide 2^32 would otherwise
  introduce.
- Guarantees at least one character from each *enabled* class, if the
  requested length allows it (common password-policy expectation),
  then shuffles the whole result so those aren't predictably placed
  first.

## Symbol set

Deliberately a conservative subset (`!@#$%^&*()-_=+[]{}`), not every
symbol `components/usb`'s USB HID layer can type. All are confirmed
mapped in `keycode_map.cpp`'s `ascii_to_hid()`; the ones left out
(backtick, backslash, pipe, quotes) are more prone to odd behavior
across keyboard layouts than the risk is worth for a generated
password.

## Please verify on first build

- `MIN_LENGTH`/`MAX_LENGTH` (4/64) are reasonable-looking bounds, not
  spec'd anywhere in the project's own docs.
- The symbol set above is a judgment call, not something REQUIREMENTS.md
  or WEB.md pins down -- revisit if it turns out too restrictive (or
  too permissive for some target application) in practice.
