# components/vault

## Scope of this pass

Built: `vault_model` (VaultEntry + validation), `vault_repository`
(TLV on-disk format + CRUD, the sole caller of `storage::vaultfile`),
`vault` (VaultService facade -- lock-gated, publishes
`event_bus::Category::Vault` events).

**Not built yet, deliberately deferred** (per the build plan's own
file breakdown, and to keep this pass a reviewable size):
`vault_backup`, `vault_import`, `vault_export`. Each of those, per
`docs/STORAGE.md` section 11, must call `security::permission::check()`
(`Operation::ExportVault` / `RestoreBackup`) before doing anything --
that's their job to wire up when built, not `vault`'s.

## Three decisions fixed before writing any of this (see chat)

1. **`storage::vaultfile`** (new, additive-only file in the
   already-built `components/storage`) is the sole sanctioned way to
   read/write `vault.db`, per `docs/STORAGE.md`'s access rule
   (`VaultService -> StorageService -> LittleFS`; `GUI -> LittleFS` or
   any other shortcut is explicitly forbidden there).
   `vault_repository.cpp` is the only file in this component (and the
   only file in the whole firmware) that calls it.
2. **TOTP**: `VaultEntry::totp_secret` exists (storage only). No code
   generates a TOTP code from it -- there's no trustworthy time source
   yet (no RTC chip, no NTP/Wi-Fi). Building RFC 6238 generation
   against fake/unreliable time would be worse than not having it;
   this is a deferred, later step once a real time source exists.
   **Consequence worth remembering**: since vault.db isn't encrypted
   (see `components/security/README.md`), `totp_secret` sitting there
   in plaintext is exactly as exposed to a physical flash dump as
   `password` is -- not a smaller or separate risk.
3. **`vault.db` format**: custom binary, owned entirely by this
   component per `STORAGE.md` ("формат базы данных определяется
   компонентом Vault"). Header (magic + `VAULT_FORMAT_VERSION` + entry
   count) followed by length-prefixed, TLV-encoded entries -- see the
   file-level comment in `vault_repository.hpp` for the exact byteb
   layout. TLV means a future format version can add fields without
   breaking this reader (unknown fields/entries are skipped by their
   declared length, not rejected). No JSON library, no external
   dependency.

## Design notes

- `vault::repository::` (not `vault::`) holds `VaultRepository`'s
  functions -- kept in a sub-namespace specifically to avoid a name
  collision with `vault::init()`/`vault::list_entries()`/etc. in the
  public facade (`vault.hpp`), which has the same-shaped names by
  design (a thin wrapper around the repository).
- Every `vault::` facade operation requires
  `security::lock::state() == Unlocked`. This is the baseline gate
  for "can touch the vault at all" -- it does NOT call
  `security::permission::check()` for plain CRUD, because none of
  `PermissionManager`'s six specific operations
  (`PrintPassword`/`PrintOtp`/`ExportVault`/`ChangePin`/
  `RestoreBackup`/`WebLogin`) describe "add/edit/delete an entry" --
  those are for the specific sensitive actions named in
  `docs/REQUIREMENTS.md` 9.3, which belong to components not built yet
  (USB HID typing, backup/import/export).
- Every write (add/update/remove) re-encodes and persists the ENTIRE
  vault immediately (no batching/dirty-flag). Simple and safe; revisit
  if a vault with hundreds of entries makes this noticeably slow on
  real hardware -- untested at that scale.
- Rollback on write failure: `add()`/`update()`/`remove()` all restore
  the in-memory cache to its prior state if
  `storage::vaultfile::write_all()` fails, so the cache never disagrees
  with what's actually on disk.

## Please verify on first build

- Nothing new dependency-wise beyond components already built and
  confirmed (`storage`, `security`, `event_bus`) -- no new ESP-IDF
  component names to double-check this time.
- `std::string`/`std::vector` are used here (unlike most components so
  far, which stuck to fixed C buffers) -- a deliberate fit for
  variable-length account data. Flag if this turns out to be a
  heap-fragmentation concern once the vault sees real, sustained use;
  untested at scale.
