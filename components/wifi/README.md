# components/wifi

Phase A of docs/WEB.md's networking subsystem -- `WiFiService`, per
docs/developer/07_wifi.md. Only Station and Access Point modes, one at
a time (AP+STA simultaneously is that document's own stated future
work, not built here).

## What's here

- `wifi_service.hpp/.cpp` -- `init()` brings up the ESP-IDF Wi-Fi/
  netif/event-loop machinery once; `apply_settings()` reads
  `settings::all().wifi` (mode + SSID/password for STA and AP) and
  actually starts/switches the radio, safe to call repeatedly (e.g.
  every time the user saves the Wi-Fi settings screen -- tears down
  whatever was running first).
- State changes publish on `event_bus::Category::Wifi`
  (`WifiEventId`) -- Started/Stopped, Connecting/Connected/
  Disconnected/ConnectionFailed (Station), ApStarted/ApStopped/
  ApClientJoined/ApClientLeft (Access Point).
- `settings::WifiSettings` (mode, sta_ssid/sta_password, ap_ssid/
  ap_password) added to `settings_types.hpp` -- REQUIREMENTS 11.1 lists
  AP/Station/Captive Portal support without specifying settings
  fields, so this is a reasonable placeholder shape, not something
  pinned down in the docs.

## Explicitly NOT here (see docs/WEB.md)

- `WebService`, HTTP server, REST API, Web UI, Captive Portal -- a
  separate, much larger phase, not started.
- AP+STA simultaneous mode.
- Static IP / advanced network configuration -- DHCP only, both as
  a client (Station) and as the AP's own DHCP server (ESP-IDF's
  `esp_netif_create_default_wifi_ap()` sets this up automatically with
  its usual defaults).

## Please verify on first build

- `esp_wifi`/`esp_netif`/`esp_event` in `CMakeLists.txt`'s `REQUIRES`
  are standard, stable ESP-IDF component names -- not expected to be a
  problem the way `esp_app_format` was, but not actually build-tested
  on this project's toolchain yet either. If the build disagrees, same
  fix as always: check the exact component name in the error.
- `MAX_STA_RETRIES` (5) and the AP's `max_connection` (4) are both
  placeholders -- neither REQUIREMENTS.md nor WEB.md/07_wifi.md specify
  a number.
- `wifi_service.cpp`'s disconnect-reason-to-string mapping
  (`describe_disconnect_reason()`) only covers a handful of
  `wifi_err_reason_t` values explicitly (auth failure, AP not found);
  everything else falls back to a generic "connection failed" message.

## Security note

`settings::WifiSettings::sta_password`/`ap_password` sit in NVS in
plaintext, same exposure as every other secret in this project until
Flash Encryption is actually turned on (see components/security's
scope note and SystemInfoScreen's Flash Encryption status row) -- not
a separate, smaller risk than the vault password/PIN situation.
