# components/web

Phase B of docs/WEB.md's networking subsystem -- `WebService`. Step 1
(HTTP server + PIN login) plus step 2 (REST CRUD for the vault itself)
are both here now. See web_service.hpp's file comment for the full
"SHARED SESSION MODEL" reasoning (login calls `security::lock::unlock()`
directly, the same call the on-device LockScreen makes -- confirmed
with the project owner as the intended design, including its
consequences: an already-Unlocked device is reachable to any web
client with no separate PIN check, there's no logout endpoint, and a
wrong web PIN counts toward the same failure counter as on-device
attempts, but reaching the threshold via web disables WiFi instead of
wiping the vault -- see "What's here" below).

## What's here

- `GET /` -- the full single-page Web UI (`web_app_html.hpp`): PIN
  login, then a flat list of accounts, tap one to view its fields
  (password/TOTP secret masked behind a "Show" toggle), Edit/Delete,
  and a "+ New" button for creating an entry. No framework, one
  embedded HTML/CSS/JS page, calling the REST endpoints below via
  relative `fetch()` paths (so it keeps working whether or not a
  secret-word prefix is active). Checks `GET api/v1/auth/status` on
  load and goes straight to the account list if already Unlocked.
- `POST /api/v1/auth/login` -- body `{"pin":"123456"}` ->
  `security::lock::unlock()`. On `VerifyResult::WipeRequired`,
  deliberately does NOT mirror `ui::screens::LockScreen`'s
  vault/PIN wipe -- instead disables WiFi (persisted via
  `settings::set_wifi()`, stays off across reboots) since a remote
  brute-force attempt doesn't warrant a destructive, irreversible
  response the way repeated on-device guesses might.
- `GET /api/v1/auth/status` -- `{"status":"ok","data":{"authenticated":bool}}`,
  reading `security::lock::state()` directly (shared session, no
  separate token to check).
- `web_vault_routes.cpp` -- REST CRUD for individual vault entries,
  all requiring Unlocked (same shared-session check as the endpoints
  above), no additional `security::permission::check()` beyond that
  (matches `vault::VaultService`'s own stance that plain CRUD isn't
  one of the six specifically-gated operations -- see `vault.hpp`):
  - `GET /api/v1/entries` -- list, summary fields only (no password/
    notes/totp_secret -- adds `has_otp` instead), capped at 256
    entries (same placeholder-cap reasoning as the on-device UI's
    own MAX_ROWS/SCAN_CAP constants).
  - `GET /api/v1/entry?id=N` -- single entry, every field.
  - `POST /api/v1/entry` -- create; JSON body with any of
    login/password/url/notes/totp_secret/category/favorite (`login`
    required, everything else optional/empty by default); returns
    the new id. `id`/`created_at`/`updated_at` in the request body
    are ignored -- server-owned always.
  - `PUT /api/v1/entry?id=N` -- update; same body shape, replaces the
    whole entry (not a partial patch -- omitted fields become empty,
    matching how AccountEditScreen's own Save behaves).
  - `DELETE /api/v1/entry?id=N` -- delete.
  - Both create and update run the entry through `vault::validate()`
    before persisting, same as every other path into the vault.
- JSON response shape follows docs/WEB.md section 7 exactly:
  `{"status":"ok","data":{...}}` / `{"status":"error","message":"..."}`.
- Optional secret-word URL path prefix, from KeyKeeper 1.90's own
  "secretword" feature -- `settings::SecuritySettings::secret_word`,
  edited from `WifiSettingsScreen` (grouped there, not in Security
  Settings, even though the field lives in `SecuritySettings` -- see
  that screen's own file comment). When set, every route here is
  registered as `/<secret_word>/<path>` instead of `/<path>` --
  `web_json_helpers.hpp`'s `build_prefixed_path()` builds these at
  registration time in `web::start()`. Changing the word calls the new
  `web::restart()` (stop + start) so it takes effect immediately.
  `ui::screens::QuickScreen`'s `OkShort` ("Print URL") types the
  resulting full address via USB HID, matching 1.90's own behavior
  exactly (including working while Locked -- a URL isn't sensitive
  vault data the way Print Password is).

## Explicitly NOT here yet

- Search over REST (the on-device `SearchScreen`'s substring-match
  logic isn't exposed as an endpoint, and the Web UI's account list
  has no search/filter box yet either).
- Backup/restore/settings over REST -- WEB.md section 7's full
  "Supported Operations" list also includes these; only entry CRUD is
  done, and the Web UI only covers what REST exposes.
- Captive Portal.
- HTTPS -- see the security note below and web_service.hpp's file
  comment.
- Waiting for a Wi-Fi connection before starting the HTTP server --
  `start()` is called unconditionally from `app_system` right now (see
  below); a reasonable simplification for this step, not a considered
  decision that ordering never matters once real data is exposed.

## Please verify on first build

- **cJSON comes from `espressif/cjson` via `idf_component.yml`, NOT a
  `REQUIRES json` line.** Confirmed directly against ESP-IDF v6.0.2's
  own migration notes: the built-in `json` component was removed in
  v6.0. This is the same class of "component moved/renamed" issue as
  `esp_app_format` earlier in this project, caught before the build
  this time instead of after.
- `esp_http_server` in `REQUIRES` is a standard ESP-IDF component
  name, not separately verified against this project's toolchain yet.
- `httpd_config_t.max_uri_handlers` is bumped to 16 in
  `web_service.cpp`'s `start()` -- the default is exactly 8, and
  root/login/status (3) + the 5 vault CRUD routes already hit that
  boundary with zero headroom for anything added later (search,
  backup/restore/settings). Not yet build-confirmed that 16 itself is
  sufficient once more routes exist.
- The HTTP server is started unconditionally in `app_system`'s boot
  sequence, right after WiFi -- not gated on Wi-Fi actually having an
  IP yet. Untested whether `httpd_start()` behaves fully as expected
  with no network interface up at all versus one that's merely not
  yet connected; flag if this causes a problem.

## Security note

The login endpoint sends the PIN in a plain HTTP POST body -- no TLS.
This is a real credential (not just data at rest) crossing the network
in cleartext, on top of the already-documented at-rest exposure of
vault.db/Wi-Fi credentials pre-Flash-Encryption. The vault CRUD
endpoints above have the SAME exposure for every field they carry
(passwords/TOTP secrets included) -- anyone able to observe the
network traffic between a browser and this device can read or
intercept vault data in the clear, not just the login PIN. docs/WEB.md
itself defers HTTPS to a future stage ("на текущем этапе... в
доверенной локальной сети") -- this component follows that, doesn't
add its own opinion on top.
