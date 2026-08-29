#pragma once

#include <cstdint>

// =============================================================================
// system::logger
//
// Small application-level logging facade.
//
// Lower-level components continue to use their own ESP_LOGx() calls.
// This facade is intended for messages that describe the state of the
// complete application rather than one particular hardware component.
//
// It deliberately does not introduce a second logging backend.
// ESP-IDF logging remains the actual implementation.
// =============================================================================

namespace app_system::logger {

/**
 * @brief Initialize the application logger facade.
 *
 * Safe to call once; repeated calls are harmless.
 */
bool init();

bool is_initialized();

/**
 * @brief Log that a boot stage has started.
 */
void boot_stage(const char* name);

/**
 * @brief Log successful application initialization.
 */
void ready();

/**
 * @brief Log an application-level error.
 */
void error(const char* message);

/**
 * @brief Log an informational application-level message.
 */
void info(const char* message);

/**
 * @brief Log the current application state.
 */
void state(uint8_t runtime_state, bool ready, uint32_t error_flags);

} // namespace app_system::logger