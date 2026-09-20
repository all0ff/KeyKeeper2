#pragma once

#include <cstdint>

// =============================================================================
// imu -- QMI8658 6-axis IMU (3-axis accelerometer + 3-axis gyroscope),
// onboard this Waveshare ESP32-S3-LCD-1.47B per its own official specs
// (confirmed directly: product page, user manual, AND the board's own
// schematic PDF, which shows it as U3 QMI8658 wired via I2C). Only the
// accelerometer is used here -- for auto-rotate, gravity's direction
// alone is enough, the gyroscope isn't needed.
//
// PINS: SDA=GPIO48, SCL=GPIO47 -- confirmed directly from the
// schematic's own GPIO summary table, which has a dedicated "IMU"
// column explicitly naming IMU_SDA/IMU_SCL against these two pins (a
// clearer copy of the table than an earlier version of this file had
// access to). IMU_INT1=GPIO13, IMU_INT2=GPIO12 are also on that table
// but unused here -- polling every ~400ms (see
// AUTO_ROTATE_POLL_INTERVAL) is more than adequate for orientation,
// no need for interrupt-driven reads.
//
// An earlier version of this file guessed GPIO46/GPIO47 instead (the
// only two GPIOs with no assigned function on a DIFFERENT,
// IMU-column-less copy of the summary table) -- confirmed WRONG on
// real hardware (a "QMI8658 not found" boot warning): GPIO46 is
// actually LCD_BL, the backlight, not IMU-related at all.
//
// I2C address: still tries both 0x6B (SA0 high) and 0x6A (SA0 low) --
// the SA0 strap level wasn't legible even on the clearer schematic,
// and trying both is cheap regardless.
// =============================================================================

namespace imu {

/**
 * @brief Probe for the QMI8658 (trying both SDA/SCL pin orderings and
 *        both possible I2C addresses -- see this file's own comment)
 *        and, if found, enable and configure its accelerometer.
 *
 * Not a hard requirement for anything else in this firmware -- if this
 * returns false (no IMU found, or this board revision doesn't
 * actually have one populated), auto-rotate simply stays unavailable;
 * manual 0°/180° orientation still works regardless.
 */
bool init();

bool is_present();

/**
 * @brief Raw accelerometer reading, 16-bit signed per axis (see
 *        QMI8658's own datasheet for the exact g-per-LSB scale at
 *        the configured range -- this project only ever compares
 *        these against each other/zero to guess orientation, never
 *        converts to real g units, so the exact scale doesn't
 *        matter here).
 *
 * @return false if is_present() is false, or the read itself failed.
 */
bool read_accel(int16_t& x, int16_t& y, int16_t& z);

/**
 * @brief Starts a low-priority background task that polls the
 *        accelerometer every ~400ms and calls
 *        lvgl_port::set_rotation() when the device's physical
 *        orientation flips -- see that function's own comment for
 *        which axis/threshold decides "flipped". Only meaningful
 *        while settings::GeneralSettings::orientation ==
 *        Orientation::Auto; ui::screens::GeneralSettingsScreen calls
 *        this when that's selected and stop_auto_rotate() otherwise
 *        (or on any other orientation change) -- this does not watch
 *        the setting itself.
 *
 * No-op (returns false) if is_present() is false. Safe to call again
 * while already running (restarts).
 */
bool start_auto_rotate();

void stop_auto_rotate();

bool is_auto_rotate_running();

} // namespace imu
