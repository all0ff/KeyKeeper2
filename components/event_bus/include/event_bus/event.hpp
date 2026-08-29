#pragma once

#include <cstdint>

// =============================================================================
// event_bus -- event.hpp
//
// The generic event type carried by the bus, and the six top-level
// categories from the plan (Input, Display, Storage, Vault, Power,
// System).
//
// event_bus is deliberately generic and does NOT depend on
// input/display/storage/vault/power (no REQUIRES on any of them) --
// that would create a dependency cycle once those components
// eventually publish onto the bus. Instead, each category's specific
// event IDs are owned by that category's own component (e.g. an
// id published under Category::Input is expected to be one of
// input::EventType, cast to uint32_t) and documented there, not here.
// The one exception is System, which has no other owning component --
// its IDs are defined in this file.
//
// Scope note: this component builds and tests the bus itself. Wiring
// input/power/storage to actually PUBLISH onto it is deferred to
// when a consumer (gui/system, built later per the plan) needs it --
// see README.md.
// =============================================================================

namespace event_bus {

enum class Category : uint8_t
{
    Input,
    Display,
    Storage,
    Vault,
    Power,
    System,
};

/// IDs for Category::System. No other component owns "System" events,
/// so they're defined directly here rather than deferred to an owning
/// component. This is also the catch-all category for cross-cutting
/// application-level events that aren't tied to one hardware
/// subsystem (see docs/SOFTWARE.md's EventBus examples: DeviceLocked,
/// PinChanged, SettingsChanged, BackupStarted, ... all fall here).
enum class SystemEventId : uint32_t
{
    BootComplete,
    ErrorReported,
    ErrorCleared,
    SettingsChanged,
    DeviceLocked,
    DeviceUnlocked,
    PinChanged,
};

/**
 * @brief Small fixed-size payload attached to an event.
 *
 * Deliberately not a pointer/dynamic allocation: events are copied by
 * value through a FreeRTOS queue, and every payload shape needed so
 * far (an encoder delta, a power state, an error flag) fits in 8
 * bytes. If a future event genuinely needs more, publish an id that
 * tells subscribers where to look up the full data themselves (e.g.
 * "vault entry index N changed") rather than growing this union.
 */
union Payload
{
    int32_t i32;
    uint32_t u32;
    struct
    {
        int32_t a;
        int32_t b;
    } pair;
    uint8_t bytes[8];
};

struct Event
{
    Category category;

    /// Category-specific event id. See the file-level comment for
    /// where each category's IDs are defined.
    uint32_t id;

    Payload payload{};

    /// esp_timer-based millisecond timestamp, filled in by publish().
    uint32_t timestamp_ms = 0;
};

} // namespace event_bus
