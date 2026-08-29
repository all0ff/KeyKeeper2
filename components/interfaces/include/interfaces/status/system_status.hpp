#pragma once

#include <cstdint>

// =============================================================================
// interfaces::status
//
// Pure observational status layer. Reads the already-exposed
// is_initialized()/status() of bsp/display/input/power/storage and
// combines them into one snapshot -- it does NOT own a task, does NOT
// call anyone's init(), and never blocks. This is a contract/view
// layer, not a service: nothing here manages the components it
// reports on.
//
// error_flags is the one piece of state this layer DOES own, because
// interfaces has no way to distinguish "not initialized yet" from
// "initialization was attempted and failed" just by looking at
// is_initialized() == false. Whoever actually calls a subsystem's
// init() and sees it fail is responsible for calling report_error().
// =============================================================================

namespace interfaces::status {

enum ErrorFlag : uint32_t
{
    ERROR_NONE    = 0,
    ERROR_BSP     = 1u << 0,
    ERROR_DISPLAY = 1u << 1,
    ERROR_INPUT   = 1u << 2,
    ERROR_POWER   = 1u << 3,
    ERROR_STORAGE = 1u << 4,
};

struct SystemStatus
{
    bool bsp_ready = false;
    bool display_ready = false;
    bool input_ready = false;
    bool power_ready = false;
    bool storage_ready = false;

    /// Informational, not an error: the microSD card is removable by
    /// design (see components/storage). Only meaningful when
    /// storage_ready is true.
    bool sdcard_present = false;

    bool has_error = false;
    uint32_t error_flags = ErrorFlag::ERROR_NONE;
};

/**
 * @brief Re-query every low-level component and return the fresh
 *        snapshot.
 *
 * Safe to call at any point in boot (even before some components are
 * initialized -- their *_ready fields will just read false). Carries
 * forward whatever error flags are currently set via report_error().
 */
const SystemStatus& refresh();

/**
 * @brief The last snapshot computed by refresh(), without
 *        recomputing it.
 *
 * Before the first refresh() call, this returns a snapshot with
 * everything false/zero.
 */
const SystemStatus& snapshot();

/**
 * @brief Record that a specific subsystem failed to initialize.
 *
 * Call this from wherever that subsystem's init() was actually
 * invoked and returned false -- interfaces has no way to know that on
 * its own.
 */
void report_error(ErrorFlag flag);

void clear_error(ErrorFlag flag);

} // namespace interfaces::status
