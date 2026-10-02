# components/rtc_time

## Role

Trustworthy wall-clock time, when available -- and a real, standing
limitation when it isn't, not a temporary gap to paper over.

This board has **no battery-backed RTC chip**. The only source of real
time is NTP over Wi-Fi (Station mode):

- Without Wi-Fi Station configured and actually connected at least
  once since boot, there is no trustworthy time at all.
- A full power loss resets the SoC's own internal clock to zero --
  there's no coin-cell-backed RTC keeping it running across power
  cycles the way a dedicated RTC chip would. Every boot needs a fresh
  sync before `is_synced()` becomes true again.
- An external RTC chip added to the board later would remove this
  limitation; not present on the currently supported hardware.

This component owns exactly that: kicking off an SNTP sync when Wi-Fi
Station connects, and tracking whether one has ever succeeded this
boot.

## Structure

Single free-function namespace, `rtc_time`:

- `init()` -- sets up the underlying SNTP client. Call once at boot,
  **after** `wifi::init()` -- confirmed on real hardware that the
  order matters, not just a style suggestion: `wifi::init()` is what
  actually calls `esp_netif_init()` + `esp_event_loop_create_default()`,
  and `init()`'s own `esp_event_handler_register()` call needs that
  default event loop to already exist. Called before `wifi::init()`,
  it fails outright every boot.
- `start_sync()` -- (re)starts an SNTP sync attempt. Safe to call
  repeatedly (e.g. every Wi-Fi Station reconnect) -- restarts cleanly
  whether idle, already running, or already synced, which also helps
  catch client-side clock drift on a long-running session.
- `is_synced()` -- true once at least one sync has succeeded *this
  boot*. Everything that needs real time (`components/totp` in
  particular) must check this before trusting `unix_time()` -- an
  unsynced clock reads as roughly zero (Jan 1 1970), not a plausible
  current time, so using it unchecked wouldn't just be imprecise, it
  would silently generate a TOTP code for entirely the wrong moment.
- `unix_time()` -- current UTC time as Unix seconds-since-epoch.
  Meaningless unless `is_synced()` is true. A thin, explicitly-named
  wrapper over the standard library's own `time()` (ESP-IDF's SNTP
  implementation calls `settimeofday()` internally once synced) --
  not a separate clock of its own.

## Dependency direction

`rtc_time` does **not** depend on `wifi` -- `wifi` depends on *this*
component instead (its `apply_settings()` calls `start_sync()` from
its own `IP_EVENT_STA_GOT_IP` handler). That direction is deliberate:
anything that only needs time (`components/totp`) can depend on just
`rtc_time`, not the whole networking stack.
