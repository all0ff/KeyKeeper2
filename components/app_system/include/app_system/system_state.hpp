#pragma once

#include <cstdint>

// =============================================================================
// system::state
//
// Runtime state of the complete KeyKeeper2 application.
//
// This is an application-level snapshot. It does not replace
// interfaces::status::SystemStatus, which remains the low-level
// initialization/status view of BSP, display, input, power and storage.
//
// system::state adds the application lifecycle:
//
//     Starting -> Ready
//                    |
//                    +-> Locked / Unlocked
//                    |
//                    +-> Sleeping
//                    |
//                    +-> Error
//
// The state is owned by the System component and is updated by
// system::init() and by runtime callbacks/events.
// =============================================================================

namespace app_system::state {

enum class BootStage : uint8_t
{
    NotStarted,
    Bsp,
    Display,
    Lvgl,
    Input,
    Power,
    Storage,
    EventBus,
    Settings,
    Security,
    Vault,
    Ui,
    Ready,
    Failed,
};

enum class RuntimeState : uint8_t
{
    Starting,
    Ready,
    Locked,
    Unlocked,
    LightSleep,
    DeepSleep,
    Error,
};

struct Snapshot
{
    BootStage boot_stage = BootStage::NotStarted;
    RuntimeState runtime = RuntimeState::Starting;

    bool initialized = false;
    bool ready = false;

    uint32_t error_flags = 0;
};

/**
 * @brief Return the current application state.
 */
const Snapshot& snapshot();

/**
 * @brief Set the current boot stage.
 *
 * Intended for the System component only.
 */
void set_boot_stage(BootStage stage);

/**
 * @brief Set the runtime state.
 *
 * Intended for the System component only.
 */
void set_runtime(RuntimeState state);

/**
 * @brief Mark the application as successfully initialized.
 */
void set_ready();

/**
 * @brief Record a system-level error flag.
 */
void report_error(uint32_t flag);

/**
 * @brief Clear a previously recorded system-level error flag.
 */
void clear_error(uint32_t flag);

/**
 * @brief Reset the state to its initial values.
 *
 * Primarily useful for tests. Normal firmware operation does not
 * restart the System component after successful initialization.
 */
void reset();

} // namespace app_system::stateЫ