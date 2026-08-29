#pragma once

#include <cstdint>

// =============================================================================
// bsp::board
//
// Static identification and hardware parameters of the KeyKeeper2 board.
//
// This header describes *what the board is* (flash/PSRAM size, display
// panel, microSD bus width, revision string). It does not perform any
// hardware initialization — see bsp.hpp for the BSP entry point, and the
// owning component (display, input, storage, ...) for peripheral-specific
// setup.
// =============================================================================

namespace bsp::board {

/**
 * @brief Flash memory parameters.
 */
struct FlashInfo
{
    static constexpr uint32_t size_mb = 16;
};

/**
 * @brief PSRAM parameters.
 *
 * The board uses an ESP32-S3R8 module with octal PSRAM.
 */
struct PsramInfo
{
    static constexpr uint32_t size_mb = 8;
    static constexpr bool octal = true;
};

/**
 * @brief LCD panel parameters.
 *
 * These describe the physical panel. SPI bus configuration and driver
 * details belong to components/display, not to bsp.
 */
struct LcdInfo
{
    static constexpr uint16_t width  = 172;
    static constexpr uint16_t height = 320;

    static constexpr const char* controller = "ST7789";

    /**
     * Panel-specific offsets/orientation required by this exact module.
     * Verified against the Waveshare ESP32-S3-LCD-1.47B reference
     * implementation. components/display applies these values; bsp only
     * publishes them so there is a single source of truth.
     */
    static constexpr uint8_t  offset_rotation = 1;
    static constexpr uint16_t offset_x = 34;
    static constexpr uint16_t offset_y = 0;
    static constexpr bool     invert_color = true;
    static constexpr bool     rgb_order = false; // false = BGR panel wiring
};

/**
 * @brief microSD interface parameters.
 */
struct MicroSdInfo
{
    static constexpr uint8_t bus_width_bits = 4;
};

/**
 * @brief Board identification.
 */
struct Identity
{
    static constexpr const char* name = "Waveshare ESP32-S3-LCD-1.47B";
    static constexpr const char* mcu  = "ESP32-S3R8";

    /**
     * Board hardware revision string. Not a firmware version.
     * Updated when the physical board design changes.
     */
    static constexpr const char* revision = "initial";
};

/**
 * @brief Aggregate, read-only description of the board.
 *
 * Components should read these values instead of re-declaring hardware
 * constants locally.
 */
struct Info
{
    Identity    identity;
    FlashInfo   flash;
    PsramInfo   psram;
    LcdInfo     lcd;
    MicroSdInfo microsd;
};

/**
 * @brief Return the static description of this board.
 */
const Info& info();

} // namespace bsp::board
