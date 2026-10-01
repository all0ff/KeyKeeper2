# espp/ws-s3-lcd-1-47 (reference copy, not built)

This is a copy of Espressif's own `espp/ws-s3-lcd-1-47` component
(`base_component`, `display`, `display_drivers`, `i2c`, `interrupt`,
`task`, `input_drivers`), kept here for reference only.

**Not part of the build.** `components/display` in this project uses
LovyanGFX instead (see `components/display/CMakeLists.txt`). No
`CMakeLists.txt` in `components/` or `main/` references anything
under this directory, and it is not listed in any component's
`REQUIRES`/`PRIV_REQUIRES`.

It was pulled in while bringing up the Waveshare ESP32-S3-LCD-1.47B
board, as a working reference for how Espressif's own example
initializes this panel, before the project settled on LovyanGFX for
the actual display driver. Kept for that reference value; safe to
delete entirely if that's no longer useful.

Previously lived under `docs/hardware/schematics/vendor/`, mixed in
with this board's actual schematic documentation (PDF/HTML/PNG) --
moved here to separate "third-party reference source code, unused"
from "documentation," which now lives at
`docs/hardware/schematics/vendor/` on its own.

Source:
- https://components.espressif.com/components/espp/ws-s3-lcd-1-47/versions/1.0.20/examples/example
- https://components.espressif.com/components/espp/ws-s3-lcd-1-47/versions/1.0.20/readme
- https://components.espressif.com/components/espp/ws-s3-lcd-1-47/versions/1.0.20/dependencies
