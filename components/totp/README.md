# components/totp

## Role

RFC 6238 (TOTP) code generation for vault entries' two-factor secrets.

Algorithm: standard TOTP with SHA-1 (the near-universal default every
authenticator app and 2FA setup QR code assumes unless it explicitly
says otherwise -- `vault::VaultEntry::totp_secret` has no separate
"algorithm"/"digits"/"period" fields, so this isn't configurable per
entry), 30-second time step, 6-digit codes, dynamic truncation per
RFC 4226 section 5.3. HMAC-SHA-1 via PSA Crypto's `psa_mac_compute()`
-- the same PSA Crypto stack this project already relies on for PIN
hashing (`security::pin`'s PBKDF2), just a different algorithm.

## Structure

Single free function namespace, `totp`:

- `generate(secret_base32, out_code, out_code_size)` -- the current
  6-digit code for a Base32-encoded secret (RFC 4648 -- the standard
  shape a TOTP QR code or "manual entry" string comes in, e.g.
  `JBSWY3DPEHPK3PXP`; decoded internally, never stored decoded).
  Case-insensitive; `=` padding and internal spaces/dashes are
  tolerated and ignored, since some apps format manual-entry keys with
  either. Fails cleanly (returns `false`, leaves `out_code` untouched)
  if `rtc_time::is_synced()` is false, the secret is empty or contains
  characters outside the Base32 alphabet, or the output buffer is too
  small (`out_code_size` must be >= 7).
- `seconds_remaining()` -- seconds left in the current 30-second step
  (0-29), for a UI countdown/auto-refresh indicator next to a
  displayed code. Based purely on `rtc_time::unix_time() % 30`, so
  meaningful even before `generate()` has been called this step --
  still requires `rtc_time::is_synced()` to mean anything, same
  caveat as `generate()` itself.

## Dependency

Needs a trustworthy clock -- see `components/rtc_time`'s own README
for why this board has none by default (no battery-backed RTC chip,
NTP-over-Wi-Fi only) and what "trustworthy" means here. `generate()`
checks `rtc_time::is_synced()` itself; callers don't need to check it
separately first, but should treat a `false` return as "not synced
yet" rather than assuming the secret itself was necessarily invalid.
