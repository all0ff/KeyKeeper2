# components/bsp

## Role

Board support package for the Waveshare ESP32-S3-LCD-1.47B. First
layer `app_main()` initializes, and must not depend on any other
KeyKeeper2 component -- everything else depends on this one, directly
or indirectly, for board identity and pin numbers.

Deliberately narrow scope: `bsp` identifies the board (chip/flash/PSRAM
parameters, panel type, microSD bus width, revision string -- see
`board.hpp`) and centralizes the GPIO map (`pins.hpp`). It does **not**
configure the LCD, the encoder/buttons, the microSD bus, or the RGB
LED -- each of those is owned and initialized by its own component
(`display`, `input`, `storage`, `app_system`) using the pin
definitions this component only *publishes*. That split keeps
peripheral ownership unambiguous: exactly one component ever
configures a given GPIO.

## Structure

- **`bsp`** (`bsp.cpp`) -- the entry point: `init()`,
  `is_initialized()`, plus `board_name()`/`board_revision()` as thin
  convenience wrappers over `bsp::board::info()`.
- **`bsp::board`** (`board.cpp`) -- static identification:
  `FlashInfo` (16 MB), `PsramInfo` (8 MB, octal -- this board uses an
  ESP32-S3R8 module), LCD panel parameters, microSD bus width, and the
  board's own revision string. Describes *what the board is*; performs
  no hardware initialization itself.
- **`bsp::pins`** (`pins.hpp`, header-only) -- the single source of
  truth for every GPIO number on this board. No other component may
  hardcode a pin number; everything includes this header and
  references `bsp::pins::<NAME>` instead. Each pin group's comment
  documents which component owns (is allowed to configure/drive) it --
  `bsp` itself configures none of them, including `BOOT` (GPIO0, the
  strapping pin for the boot/download function), which is published
  here only so nothing else accidentally reuses that GPIO number, not
  because `bsp` drives it.

## Gotchas

- Changing a pin assignment means editing exactly one file
  (`pins.hpp`) -- if a grep for a raw GPIO number turns up a hit
  outside this header, that's a layering violation worth fixing, not
  a second source of truth to keep in sync.
- `bsp::init()` must run before any component that calls
  `bsp::pins::*` -- it doesn't configure those pins itself, but other
  components' own init functions assume `bsp` has already run.
