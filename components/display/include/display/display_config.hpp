#pragma once

#include <cstdint>

#include "driver/spi_master.h"

// =============================================================================
// display::params
//
// Parameters owned by the display component: SPI bus timing/DMA, backlight
// PWM, and LVGL buffering/task tuning.
//
// Panel electrical parameters that describe the physical board (offsets,
// rotation, color inversion, RGB/BGR order, native panel memory size) are
// NOT duplicated here — they come from bsp::board::info().lcd, which is
// the single source of truth for those values (see bsp/board.hpp).
//
// The exact values below (SPI frequencies, offset_rotation, dummy-read
// settings) were verified against a working hardware reference
// implementation for this exact board (Waveshare ESP32-S3-LCD-1.47B,
// USBArmyKnife project, WAVESHARE_ESP32_S3_LCD_147 build target) and
// should not be changed without re-testing on real hardware.
// =============================================================================

namespace display::params {

/**
 * @brief SPI bus parameters for the LCD link.
 */
struct SpiBus
{
    static constexpr spi_host_device_t host = SPI3_HOST;

    static constexpr int spi_mode = 0;

    static constexpr uint32_t freq_write_hz = 27'000'000;
    static constexpr uint32_t freq_read_hz  = 16'000'000;

    /**
     * The panel has no MISO line; register/pixel readback (if ever used)
     * happens over the MOSI line in 3-wire mode.
     */
    static constexpr bool spi_3wire = true;

    /**
     * The LCD has its own dedicated SPI3 bus. microSD uses the separate
     * SDMMC peripheral (4-bit), not this SPI bus, so it is not shared.
     */
    static constexpr bool bus_shared = false;

    static constexpr int dummy_read_pixel_bits = 8;
    static constexpr int dummy_read_bits = 1;
};

/**
 * @brief Backlight PWM (LEDC) parameters.
 */
struct Backlight
{
    static constexpr bool invert = true;
    static constexpr uint32_t pwm_freq_hz = 12'000;

    /**
     * Reserved LEDC channel for the backlight. Must stay unique across
     * the firmware if other components later use LEDC PWM outputs.
     */
    static constexpr int pwm_channel = 7;
};

/**
 * @brief LVGL draw-buffer and task parameters.
 */
struct Lvgl
{
    /**
     * Number of display lines per LVGL draw buffer. Two buffers of this
     * size are allocated for double buffering. Tunable without any other
     * code change.
     */
    static constexpr uint16_t draw_buffer_lines = 40;

    static constexpr uint32_t tick_period_ms = 2;

    static constexpr uint32_t task_period_ms = 5;
    static constexpr uint32_t task_stack_size = 6144;
    static constexpr uint8_t  task_priority = 4;
};

} // namespace display::params
