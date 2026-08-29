# components/display

## External dependencies (action required before `idf.py build`)

1. **LovyanGFX** — not published on the ESP Component Registry, so
   `idf_component.yml` cannot pull it automatically. Vendor it manually:

   ```
   cd components
   git clone --depth 1 https://github.com/lovyan03/LovyanGFX.git LovyanGFX
   ```

   The cloned repo's own `CMakeLists.txt` registers it as an ESP-IDF
   component named `LovyanGFX`, matching `PRIV_REQUIRES` in this
   component's `CMakeLists.txt`.

2. **LVGL** — pulled automatically by `idf_component.yml`
   (`lvgl/lvgl ^9.2`) the first time you run `idf.py build`, provided the
   board has network access. After the first fetch it is cached under
   `managed_components/`.

3. **LVGL color depth** — run `idf.py menuconfig` →
   `Component config → LVGL configuration → Color settings` and set
   color depth to **16 bit**. This must match `LV_COLOR_FORMAT_RGB565`
   used in `lvgl_port.cpp`; a mismatch here is a common source of a
   garbled/shifted image and is not caught at compile time.

## Design notes

- `display.hpp` is the only public header. It exposes no LVGL or
  LovyanGFX types on purpose, so any component that only needs
  backlight/brightness control does not need to see the graphics
  stack.
- `display_panel.hpp` (in `src/`) is private to this component. It is
  shared between `display.cpp` and `lvgl_port.cpp` via
  `PRIV_INCLUDE_DIRS`.
- Panel electrical parameters (offsets, rotation, inversion, RGB/BGR
  order, native panel size) come from `bsp::board::info().lcd` — they
  are not redefined here, to keep a single source of truth alongside
  the pin map in `bsp/pins.hpp`.
- SPI/backlight/LVGL tuning parameters that are specific to *this*
  component live in `display_config.hpp`.

## Values carried over from the hardware reference (USBArmyKnife)

The following were taken as-is from the verified
`WAVESHARE_ESP32_S3_LCD_147` build target in USBArmyKnife, because they
are hardware-specific and were already validated on real boards:

- SPI3, 27 MHz write / 16 MHz read, mode 0, 3-wire (no MISO).
- Panel offsets: `offset_rotation=1, offset_x=34, offset_y=0,
  invert=true, rgb_order=false (BGR)`.
- Backlight: LEDC PWM, 12 kHz, inverted duty.
- Native panel memory 172x320 (portrait); with `offset_rotation=1` the
  logical drawing surface used by LVGL is 320x172 (landscape).

## Please verify on real hardware before relying on this

- **microSD pin mapping**: `bsp/pins.hpp` assigns `SD_D1=GPIO17,
  SD_D2=GPIO18`. The USBArmyKnife `platformio.ini` for this exact board
  lists `SD_MMC_D1_PIN=18, SD_MMC_D2_PIN=17` — the opposite way round.
  This doesn't affect the display component, but flag it before
  `components/storage` (microSD) is implemented — one of the two
  sources has D1/D2 swapped and it needs to be checked against the
  Waveshare schematic, not guessed.
- `cfg.readable = true` (pixel/register readback) is carried over from
  the reference but nothing in this firmware reads from the panel yet.
  If a screenshot/backup-preview feature is ever added, test read
  timing on real hardware — 16 MHz read is a reference default, not a
  value derived for this specific use.
