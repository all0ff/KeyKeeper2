# components/power

## Design

- `power.hpp/.cpp` — thin public API, forwards everything to the
  internal `Manager` singleton.
- `power_manager.hpp/.cpp` (private) — owns the background task, the
  idle timer, the callback registry, and the actual `esp_sleep`
  configuration/entry.
- Activity tracking does **not** consume from `input`'s event queue --
  it polls the non-destructive `input::last_activity_ms()` timestamp
  instead. This matters: `input::poll_event()` is a single-consumer
  queue, so if `power` also drained it, it would race the real event
  consumer (currently the smoke-test `app_main`, later the UI) for
  the same events and both would silently miss some.
- Light sleep wakes on **any** GPIO edge (OK, BACK, or either encoder
  phase) via `gpio_wakeup_enable()`. Deep sleep only wakes on **OK or
  BACK** via the RTC/EXT1 path — deliberately excludes the encoder, so
  an idle knob twitch can't power the device back on from a full
  shutdown.
- `power::request_sleep()` / the idle-timeout path are serialized with
  a mutex so they can never both call `esp_light_sleep_start()` at the
  same time.

## Please verify on real hardware / your exact ESP-IDF version

- **`esp_sleep_enable_ext1_wakeup()`** — used with
  `ESP_EXT1_WAKEUP_ANY_LOW`. This is the long-standing classic API;
  given how much the driver components have moved around in your IDF
  version (v6.0.2), double-check this function/enum name still exists
  as-is. If not, the migration guide referenced in earlier build
  errors should show its replacement.
- **`esp_hw_support` as the REQUIRES entry for `esp_sleep.h`.** I
  believe this is correct for current ESP-IDF, but if the build
  complains about a missing `esp_sleep_start`/`esp_light_sleep_start`
  symbol, that's the component to check first.
- **Backlight/LVGL behavior across light sleep is untested.** The LVGL
  tick timer (`esp_timer`, 2 ms period, in `components/display`) and
  the LEDC backlight PWM keep running as configured; entering light
  sleep should just pause the CPU until a wake GPIO fires, without
  disturbing either. Worth confirming on the real board that the
  screen doesn't flicker or the backlight doesn't glitch on
  sleep/wake — I have no way to test this without hardware.
- **`idle_timeout_ms` default (30 s)** is a placeholder, not a
  considered UX decision. Tune it once you can actually feel how long
  30 seconds is in daily use.
