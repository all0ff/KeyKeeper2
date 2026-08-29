#pragma once

#include "app_system/system_state.hpp"

// =============================================================================
// system
//
// Top-level application coordinator.
//
// System is above the individual hardware/service components. It owns
// the initialization sequence and application-level runtime coordination,
// but it does NOT reimplement the functionality of those components.
//
// Initialization order:
//
//     BSP
//       ↓
//     Display
//       ↓
//     LVGL
//       ↓
//     Input
//       ↓
//     Power
//       ↓
//     Storage
//       ↓
//     EventBus
//       ↓
//     Interfaces status
//       ↓
//     Settings
//       ↓
//     Security
//       ↓
//     Vault
//       ↓
//     UI
//       ↓
//     Ready
//
// Each subsystem remains responsible for its own implementation and
// public API. System only calls those APIs in the correct dependency
// order and records the application-level result.
//
// app_main() should eventually become a very small entry point that
// calls system::init() and then returns.
// =============================================================================

namespace app_system {

/**
 * @brief Initialize the complete KeyKeeper2 application.
 *
 * The function performs the complete startup sequence and stops at
 * the first mandatory subsystem failure.
 *
 * Safe to call once. A second call after successful initialization
 * returns true without repeating initialization.
 *
 * @return true when the application reaches the Ready state.
 */
bool init();

/**
 * @brief Return whether System has completed successfully.
 */
bool is_initialized();

/**
 * @brief Return the current application runtime state.
 */
state::RuntimeState runtime_state();

/**
 * @brief Return the current application state snapshot.
 */
const state::Snapshot& snapshot();

/**
 * @brief Request application shutdown.
 *
 * This delegates the actual power-down operation to the Power
 * component. The System component does not directly manipulate
 * ESP32 sleep registers.
 */
void shutdown();

/**
 * @brief Request the application to enter light sleep.
 *
 * The actual sleep implementation remains owned by components/power.
 */
void sleep();

} // namespace app_system