#pragma once

#include "bsp/board.hpp"

// =============================================================================
// bsp
//
// Top-level BSP entry point.
//
// bsp is the first layer initialized by app_main() and must not depend on
// any other KeyKeeper2 component. Its responsibility is strictly limited
// to:
//
//   - identifying the board (see bsp::board);
//   - initializing hardware resources that are genuinely shared and do
//     not belong to a single owning component (e.g. chip-level setup).
//
// bsp does NOT configure the LCD, the encoder/buttons, the microSD bus,
// or the RGB LED. Each of those is owned and initialized by its own
// component (display, input, storage, system) using the pin definitions
// published in bsp::pins. This keeps peripheral ownership unambiguous:
// exactly one component configures any given GPIO.
// =============================================================================

namespace bsp {

/**
 * @brief Initialize the BSP layer.
 *
 * Safe to call exactly once, before any other component is initialized.
 * Must not be called again after a successful call.
 *
 * @return true on success, false if BSP-level initialization failed.
 */
bool init();

/**
 * @brief Return whether bsp::init() has completed successfully.
 */
bool is_initialized();

/**
 * @brief Convenience accessor for the board name.
 *
 * Equivalent to bsp::board::info().identity.name.
 */
const char* board_name();

/**
 * @brief Convenience accessor for the board hardware revision.
 *
 * Equivalent to bsp::board::info().identity.revision.
 */
const char* board_revision();

} // namespace bsp
