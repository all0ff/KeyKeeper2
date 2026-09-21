#pragma once

#include "driver/gpio.h"

// =============================================================================
// bsp::pins
//
// Centralized GPIO map for the KeyKeeper2 board (Waveshare ESP32-S3-LCD-1.47B).
//
// This file is the single source of truth for pin numbers. No other
// component may hardcode a GPIO number; every component includes this
// header and references bsp::pins::<NAME> instead.
//
// Ownership of each pin group (who is allowed to configure/drive it) is
// documented next to the definition. bsp itself does not configure any
// of these pins — that is the responsibility of the owning component
// (display, input, storage, ...). bsp only defines where they are.
// =============================================================================

namespace bsp::pins {

// -----------------------------------------------------------------------
// Boot / strapping
// -----------------------------------------------------------------------

/**
 * ESP32-S3 BOOT button.
 *
 * GPIO0 is a strapping pin reserved for the boot/download function.
 * It must never be reconfigured or driven by application code.
 */
inline constexpr gpio_num_t BOOT = GPIO_NUM_0;

// -----------------------------------------------------------------------
// User input (owned by components/input)
// -----------------------------------------------------------------------

/**
 * External EC11 rotary encoder, quadrature channels.
 * Active-low, internal pull-up.
 */
inline constexpr gpio_num_t ENCODER_A = GPIO_NUM_4;
inline constexpr gpio_num_t ENCODER_B = GPIO_NUM_5;

/**
 * Encoder push button (OK / select).
 * Active-low, internal pull-up.
 */
inline constexpr gpio_num_t BUTTON_OK = GPIO_NUM_6;

/**
 * Dedicated BACK button.
 * Active-low, internal pull-up.
 */
inline constexpr gpio_num_t BUTTON_BACK = GPIO_NUM_7;

// -----------------------------------------------------------------------
// Expansion header
// -----------------------------------------------------------------------
//
// Available on the peripheral header. Not assigned to any KeyKeeper2
// application function at the current board revision.

inline constexpr gpio_num_t HEADER_GPIO8  = GPIO_NUM_8;
inline constexpr gpio_num_t HEADER_GPIO9  = GPIO_NUM_9;
inline constexpr gpio_num_t HEADER_GPIO10 = GPIO_NUM_10;
inline constexpr gpio_num_t HEADER_GPIO11 = GPIO_NUM_11;
inline constexpr gpio_num_t HEADER_GPIO12 = GPIO_NUM_12;
inline constexpr gpio_num_t HEADER_GPIO13 = GPIO_NUM_13;

// -----------------------------------------------------------------------
// microSD (owned by components/storage)
// -----------------------------------------------------------------------
//
// 4-bit SDMMC interface.
//
// D1/D2 were swapped here for a long time (D1=17, D2=18) -- flagged
// early in this project as an unresolved discrepancy against a
// third-party reference (USBArmyKnife) and never actually checked
// against Waveshare's own documentation until a real, physically
// inserted, freshly FAT-formatted 2GB card still wasn't detected at
// all. Confirmed directly against Waveshare's own wiki for this board
// family (waveshare.com/wiki/ESP32-S3-LCD-1.47 -- same TF card slot
// hardware as the -1.47B this project targets): SD_D1=GPIO18,
// SD_D2=GPIO17. With these swapped, the 4-bit bus's data-line
// assignment was wrong at the protocol level -- card init would fail
// regardless of format, size, or content, which matches the report
// exactly (format wasn't the problem).
inline constexpr gpio_num_t SD_CLK  = GPIO_NUM_14;
inline constexpr gpio_num_t SD_CMD  = GPIO_NUM_15;
inline constexpr gpio_num_t SD_D0   = GPIO_NUM_16;
inline constexpr gpio_num_t SD_D1   = GPIO_NUM_18;
inline constexpr gpio_num_t SD_D2   = GPIO_NUM_17;
inline constexpr gpio_num_t SD_D3   = GPIO_NUM_21;

// -----------------------------------------------------------------------
// RGB status LED (owned by components/system)
// -----------------------------------------------------------------------

inline constexpr gpio_num_t RGB_LED = GPIO_NUM_38;

// -----------------------------------------------------------------------
// LCD / ST7789 (owned by components/display)
// -----------------------------------------------------------------------

inline constexpr gpio_num_t LCD_RST  = GPIO_NUM_39;
inline constexpr gpio_num_t LCD_SCLK = GPIO_NUM_40;
inline constexpr gpio_num_t LCD_DC   = GPIO_NUM_41;
inline constexpr gpio_num_t LCD_CS   = GPIO_NUM_42;
inline constexpr gpio_num_t LCD_MOSI = GPIO_NUM_45;
inline constexpr gpio_num_t LCD_BL   = GPIO_NUM_46;

// -----------------------------------------------------------------------
// QMI8658 IMU (owned by components/imu)
// -----------------------------------------------------------------------
//
// Confirmed against the Waveshare ESP32-S3-LCD-1.47B schematic:
// IMU_SCL = GPIO47, IMU_SDA = GPIO48, INT1 = GPIO13, INT2 = GPIO12.
// The I2C lines are shared only with the onboard QMI8658.
inline constexpr gpio_num_t IMU_SCL  = GPIO_NUM_47;
inline constexpr gpio_num_t IMU_SDA  = GPIO_NUM_48;
inline constexpr gpio_num_t IMU_INT1 = GPIO_NUM_13;
inline constexpr gpio_num_t IMU_INT2 = GPIO_NUM_12;

// -----------------------------------------------------------------------
// UART
// -----------------------------------------------------------------------

inline constexpr gpio_num_t UART_TX = GPIO_NUM_43;
inline constexpr gpio_num_t UART_RX = GPIO_NUM_44;

// -----------------------------------------------------------------------
// Internal PSRAM
// -----------------------------------------------------------------------
//
// GPIO26-GPIO37 are used internally for octal PSRAM and are not
// available to the application. Intentionally left undefined here.

} // namespace bsp::pins
