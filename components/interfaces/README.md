# components/interfaces

Two deliberately separate things live here (see the diagram in chat
for the full rationale) -- don't merge them:

## status/ -- what's available, and in what shape

`interfaces::status` is a pull-based, read-only view over
bsp/display/input/power/storage. It owns no task, calls no
component's `init()`, and never blocks. `refresh()` re-queries every
component's own `is_initialized()`/`status()` and returns a
`SystemStatus` snapshot -- meant for startup diagnostics, a GUI status
screen, and later feeding into `event_bus`.

The one piece of real state it owns is `error_flags`: since
`is_initialized() == false` is ambiguous ("not started yet" vs.
"failed"), whoever actually calls a subsystem's `init()` and sees it
fail must call `interfaces::status::report_error(...)` explicitly --
this layer can't infer that on its own.

## channels/ -- contracts for future external transports

`Channel` / `InputChannel` / `OutputChannel` / `TransportKind` /
`ChannelState` are pure abstract interfaces for USB HID, Web/Wi-Fi,
and BLE. **None of these transports are implemented yet** -- this is
only the contract, so `vault`/`gui` can be written against
`OutputChannel`/`InputChannel` from the start instead of against a
concrete transport, and a real implementation can be dropped in later
without touching `vault`/`gui`. Header-only, no `.cpp` files, nothing
registers or owns an instance of these yet.

## Relationship to event_bus (not built yet)

`interfaces` answers "what's available and in what shape" (status) and
"what could I talk to" (channels). The upcoming `event_bus` answers
"how do things notify each other" -- a different concern, kept in its
own component on purpose rather than folded in here.
