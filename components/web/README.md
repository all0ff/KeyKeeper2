# components/web

Phase B, step 1 of docs/WEB.md's networking subsystem -- `WebService`.
This increment is deliberately narrow: an HTTP server plus PIN-based
login. See web_service.hpp's file comment for the full "SHARED SESSION
MODEL" reasoning (login calls `security::lock::unlock()` directly, the
same call the on-device LockScreen makes -- confirmed with the project
owner as the intended design, including its consequences: an already-
Unlocked device is reachable to any web client with no separate PIN
check, there's no logout endpoint, and a wrong web PIN counts toward
the same failure counter as on-device attempts, but reaching the
threshold via web disables WiFi instead of wiping the vault -- see
"What's here" below).

## What's here

- `GET /` -- a bare-bones login page (PIN field + Unlock button,
  inline CSS/JS, no framework). Not the real Web UI -- just enough to
  exercise the login endpoint from a browser without curl/Postman.
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
- JSON response shape follows docs/WEB.md section 7 exactly:
  `{"status":"ok","data":{...}}` / `{"status":"error","message":"..."}`.

## Explicitly NOT here yet

- Any REST endpoint touching the vault itself (list/view/create/edit/
  delete/search/backup/restore/settings -- WEB.md section 7's full
  "Supported Operations" list). Auth is the only thing this increment
  proves out.
- The real Web UI (a proper page beyond the bare login form).
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
- `esp_http_server`/`freertos`/`esp_system` in `REQUIRES` are standard
  ESP-IDF component names, not separately verified against this
  project's toolchain yet.
- The HTTP server is started unconditionally in `app_system`'s boot
  sequence, right after WiFi -- not gated on Wi-Fi actually having an
  IP yet. Untested whether `httpd_start()` behaves fully as expected
  with no network interface up at all versus one that's merely not
  yet connected; flag if this causes a problem.

## Security note

The login endpoint sends the PIN in a plain HTTP POST body -- no TLS.
This is a real credential (not just data at rest) crossing the network
in cleartext, on top of the already-documented at-rest exposure of
vault.db/Wi-Fi credentials pre-Flash-Encryption. docs/WEB.md itself
defers HTTPS to a future stage ("на текущем этапе... в доверенной
локальной сети") -- this component follows that, doesn't add its own
opinion on top.
