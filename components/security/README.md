# components/security

## Scope boundary (read this first)

**This is not a cryptographic layer.** Per the user's explicit decision
(overriding the original build-plan file, which named
`key_manager`/`crypto_types`/`secure_buffer`): for this version of
KeyKeeper2, `security` implements only what
`docs/ARCHITECTURE.md`'s SecurityService breakdown describes --
`PinManager`, `LockManager`, `SessionManager`, `PermissionManager`.
AES-256, Secure Element, PIN-based key derivation, and vault.db
encryption are `docs/REQUIREMENTS.md` 9.4 "Future Security" and are
**not** implemented here.

**Concretely: `vault.db` is not encrypted.** PIN verification gates
logical/software access to the firmware's own UI/API only. Anyone
with physical access to the chip who dumps the flash gets `vault.db`
in plaintext -- PIN does not protect data at rest. This is a known,
intentional limitation of the current version, not an oversight, and
it should stay visible (to you, and eventually to the user of the
device) rather than be quietly assumed away. The `set_pin()`/`verify()`
salted-SHA-256 hashing in `pin_manager.cpp` exists only so the PIN
itself isn't sitting in NVS as cleartext -- it is a single, unstretched
hash (not a KDF), and its output is never used as an encryption key.

The intended future path (per the user's own diagram) is a
`EncryptedVault`/`CryptoService` layer inserted between
`VaultService` and `StorageService` later, without changing
`VaultService`'s API -- not by retrofitting this component.

## Structure

Free-function namespaces, not classes (matching the style already
used for e.g. `input`/`power`) -- `security::pin`, `security::lock`,
`security::session`, `security::permission`, each independently
`init()`-able but with a real ordering dependency:
`pin -> session -> lock -> permission`. `security::init()`
(`security_service.cpp`) is a thin orchestrator that calls all four in
that order -- it does not wrap or duplicate their APIs, callers use
`security::pin::verify(...)` etc. directly.

- **`pin`** -- salted-hash storage/verification, failed-attempt
  counter, lockout.
- **`lock`** -- Locked/Unlocked state, starts Locked at boot
  (REQUIREMENTS 9.2), its own auto-lock idle task (independent of
  `components/power`'s light-sleep idle timer -- related but distinct
  concerns), re-reads `settings::all().security` live.
- **`session`** -- intentionally thin: one local session today, an
  `Origin` enum ready for `WebUi`/`Ota` later without an API change.
- **`permission`** -- gates the six operations from REQUIREMENTS 9.3
  (Print Password, Print OTP, Export Vault, Change PIN, Restore
  Backup, Web Login), requires Unlocked + an active session; WebLogin
  additionally checks `settings::web_ui_permissions`.

Events published (added to `event_bus`'s `SystemEventId`, matching the
`SettingsChanged` precedent): `PinChanged`, `DeviceLocked`,
`DeviceUnlocked` -- all named explicitly in `docs/SOFTWARE.md`'s
EventBus example list.

## Placeholder values -- not a considered policy

- `pin_manager.cpp`: `MAX_ATTEMPTS = 5`, `LOCKOUT_DURATION_MS = 30000`.
  Reasonable-looking defaults, not a threat-modeled anti-bruteforce
  policy. Revisit once you have an actual opinion on this trade-off
  (a longer lockout is more annoying to a legitimate user who
  fat-fingers their PIN, but also to an attacker).
- `permission_manager.cpp`'s `WebLogin` check just requires
  `web_ui_permissions != WEB_UI_NONE` (any permission bit set) -- not
  yet operation-specific (e.g. it doesn't distinguish "allowed to view"
  from "allowed to log in at all"). Revisit once a real Web UI exists
  and you know what each permission bit should actually gate.

## Please verify on first build

- **mbedtls 4.x / PSA Crypto, not legacy `mbedtls_sha256_*()`.** Your
  ESP-IDF v6.0 ships Mbed TLS 4.x, which removed the legacy
  `mbedtls_*` cryptography primitives in favor of the PSA Crypto API
  (`psa_crypto_init()` + `psa_hash_compute()`). `pin_manager.cpp` was
  updated to use that -- if your build still complains about a missing
  PSA header/symbol, check `Component config -> mbedTLS` in
  `idf.py menuconfig` for whether PSA Crypto support is enabled.
- **`esp_hw_support` as the REQUIRES entry for `esp_random.h`**
  (`esp_fill_random()`, used for the PIN salt) -- same "component
  names have moved around across ESP-IDF versions" caveat as earlier
  components.
