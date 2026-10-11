#pragma once

#include <cstdint>

// =============================================================================
// energy -- power saving and the battery
//
// One low-priority task. Every ~0.5 s it looks at how long nothing has happened and, according to
// settings::PowerSettings (menu "Energy saving"):
//   - Wi-Fi: no request to a real web page/API route and no use of the device for N minutes -> the Wi-Fi mode
//     is set to Disabled (the same as switching it off in the Wi-Fi menu; persisted, switch it on again there);
//   - BLE: no use of the device for N minutes -> the dongle link is paused (wireless::set_idle_pause). A
//     "Print" action wakes it by itself (usb::OutputSink::prepare) and waits a few seconds for the link;
//   - CPU: screen asleep and the web page idle -> the CPU runs at 80 MHz instead of 240 (needs
//     CONFIG_PM_ENABLE=y; without it this part logs once and does nothing). The 240 MHz lock is taken again at
//     the first key press.
// Nothing here puts the chip to sleep (the USB HID problems that caused that were the reason it was removed);
// "sleep on battery" is a separate, later step.
//
// The battery: a divider to GPIO8 (ADC1) and the charger's CHRG pin on GPIO9, see status().
//   Divider: battery(+) --220k-- GPIO8 --220k-- GND, with 100 nF from GPIO8 to GND (ratio 2:1).
//   CHRG (TP4056 etc., open drain, active low): pulled up inside the ESP32-S3; LOW = charging,
//   HIGH (floating) = charge finished / no charger.
// =============================================================================

namespace energy {

struct BatteryStatus
{
    bool present = false;   ///< a plausible battery voltage was measured (>= 2.5 V)
    uint8_t percent = 0;    ///< 0-100, estimated from the voltage (rough: depends on load and charging)
    uint16_t millivolts = 0; ///< battery voltage
    bool charging = false;  ///< CHRG pin is low
    bool usb_host = false;  ///< the vault's own USB cable is mounted by a computer
};

/// Starts the task and the battery measurement. Call once at the end of boot. Safe to call twice.
bool init();

/// Latest values (refreshed every few seconds). Thread-safe.
BatteryStatus battery();

/// True while the CPU is held at the reduced 80 MHz frequency.
bool cpu_reduced();

} // namespace energy
