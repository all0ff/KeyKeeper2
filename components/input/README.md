# components/input

## Design

- `encoder.hpp/.cpp` — generic EC11 quadrature decoder on the PCNT
  hardware peripheral (x4 decoding). Knows nothing about menus/UI.
- `button.hpp/.cpp` — generic debounced button with click, long-press,
  and auto-repeat. One instance per physical button. Knows nothing
  about which button it is.
- `input.hpp/.cpp` — the only public-facing piece. Owns one `Encoder`
  and two `Button` instances (OK, BACK), runs a FreeRTOS task that
  polls them every 5 ms, and translates their generic events into
  semantic `input::Event`s pushed onto a queue. Other components call
  `input::poll_event()` to consume them.

## Please verify on first build

- **PCNT header/component name.** `encoder.cpp` includes
  `driver/pulse_cnt.h` and calls `pcnt_new_unit()` /
  `pcnt_new_channel()` — the "new" PCNT driver API. This is correct
  for recent ESP-IDF 5.x, but component registration for PCNT has
  moved around between IDF releases (sometimes bundled in `driver`,
  sometimes a separate `esp_driver_pcnt` component). If the build
  fails with an unresolved `pcnt_*` symbol or missing header, add
  `esp_driver_pcnt` to `REQUIRES` in `CMakeLists.txt`.
- **Debounce/long-press/repeat timing** (`button.hpp::Config`:
  `debounce_ms=25`, `long_press_ms=600`, `repeat_interval_ms=150`) are
  reasonable defaults, not values tuned against your actual EC11/
  button hardware. Adjust after testing by feel.
- **Encoder direction.** If clockwise rotation produces negative
  deltas (backwards from what feels natural), set
  `Encoder::Config::invert = true` in `input.cpp` rather than
  rewiring — no need to touch `encoder.cpp`.
