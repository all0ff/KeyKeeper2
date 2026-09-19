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
// PINS: GPIO46/GPIO47, confirmed as the only two GPIOs with no
// assigned function anywhere else on this board (cross-checked
// against the schematic's own consolidated GPIO/LCD/SD Card/UART/
// Other table -- every other pin on that table is accounted for by
// something else already in this firmware; IO19/IO20, which also
// looked free at first glance, turned out to be the native USB D-/D+
// pins this project's own USB HID already uses).
//
// SDA vs SCL specifically (which of the two is which) could NOT be
// confirmed from the schematic text extraction -- see init()'s own
// comment for why, and how this handles that (tries both orderings
// at boot, keeps whichever one actually gets a response, rather than
// asking the person to verify this by hand).
//
// I2C address: 0x6B (SA0 high) by convention/default for this exact
// chip on Waveshare's own similar boards -- confirmed against a
// Waveshare-board-specific report of exactly this address, not just
// the chip's own generic default. init() also tries 0x6A (SA0 low)
// if 0x6B doesn't answer, same reasoning as the pin-order fallback.
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
