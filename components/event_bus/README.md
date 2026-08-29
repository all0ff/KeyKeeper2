# components/event_bus

## Design

- One FreeRTOS queue (`bus_queue`, depth 32) + one dispatcher task.
  `publish()` enqueues (non-blocking, drops the oldest event if full,
  same policy as `components/input`'s own queue). The dispatcher task
  blocks on the queue and calls every matching subscriber's `Handler`
  synchronously as each event comes off it.
- Fixed 16-slot subscriber table, same pattern as
  `components/power`'s callback registry.
- `event.hpp`'s `Event`/`Payload` are generic on purpose --
  `event_bus` has no `REQUIRES` on bsp/display/input/power/storage/
  vault, and never will, to avoid a dependency cycle once those
  components publish onto it. Each category's specific event IDs are
  owned by that category's component (e.g. an `Input` event's `id` is
  expected to be an `input::EventType` cast to `uint32_t`) and
  documented there -- `System` is the only category defined directly
  in `event.hpp`, since nothing else owns it.

## Scope decision: not wired to publishers yet

This step builds and tests the bus itself, but does **not** modify
`input`/`power`/`storage` to actually call `event_bus::publish()`.
Reasoning: those components are already written and build-confirmed
on your hardware; retrofitting them now would mean re-touching tested
code for a wiring change that has no consumer yet anyway (nothing
built so far needs to subscribe to bus events -- `power` already gets
input activity via `input::last_activity_ms()`, which still works
fine and doesn't need to change).

The natural point to wire real publishers in is when `gui`/`system`
(built later per the plan) actually need to *subscribe* to
input/power/storage events -- at that point it'll be a small, focused
change to each publisher (a handful of `event_bus::publish(...)`
calls added to already-working code), not a redesign.

To prove the bus itself works end-to-end in the meantime, the smoke-
test `app_main.cpp` publishes and subscribes to a couple of dummy
`System` events at boot -- see the updated smoke test.
