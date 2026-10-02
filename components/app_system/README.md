# components/app_system

## Role

Top-level application coordinator -- sits *above* every other
component, owns the boot sequence, and does not reimplement any
subsystem's own functionality. `app_main()` is meant to shrink down to
essentially just `app_system::init();` as the rest of the firmware
matures; it isn't there yet, but that's the direction.

Boot order (see `app_system.hpp`'s own comment for the authoritative
version if this ever drifts):

```
BSP -> Display -> LVGL -> Input -> Power -> Storage -> EventBus
    -> Interfaces status -> Settings -> Security -> Vault -> UI -> Ready
```

Each subsystem remains responsible for its own init/public API.
`app_system::init()` only calls those APIs in the right order and
records the application-level result -- it stops at the first
mandatory subsystem failure rather than limping forward.

## Structure

Three free-function namespaces:

- **`app_system`** (`app_system.cpp`) -- the orchestrator itself:
  `init()`, `is_initialized()`, `runtime_state()`, `snapshot()`,
  `shutdown()`, `sleep()`. The latter two delegate the actual
  power-down/sleep work to `components/power` -- this component does
  not touch ESP32 sleep registers directly.
- **`app_system::state`** (`system_state.cpp`) -- the runtime state
  machine: `BootStage` (one entry per subsystem in the boot order
  above, plus `Ready`/`Failed`) and `RuntimeState`
  (`Starting -> Ready -> Locked/Unlocked/LightSleep/DeepSleep/Error`).
  This is an *application-level* snapshot -- distinct from
  `interfaces::status::SystemStatus`, which is the lower-level
  init/status view of BSP, display, input, power and storage
  individually. `app_system::state` adds the lifecycle on top of that.
- **`app_system::logger`** (`logger.cpp`) -- a thin facade for
  application-level log messages (boot stage transitions, readiness,
  top-level errors), not a second logging backend. Lower-level
  components keep using their own `ESP_LOGx()` calls directly; this
  exists for messages that describe the whole application's state
  rather than one component's.

## Gotchas

- `init()` is safe to call exactly once; a second call after a
  successful first one returns `true` immediately without repeating
  any subsystem's own init. It is *not* designed to be called again
  after a *failed* first attempt -- there's no retry/reset path here,
  only `state::reset()` for tests.
- `report_error()`/`clear_error()` work on a bitmask
  (`uint32_t error_flags`), not a single error code -- multiple
  subsystems can have outstanding error flags simultaneously.
