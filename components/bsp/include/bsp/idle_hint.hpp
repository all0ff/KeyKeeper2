#pragma once

#include <atomic>

namespace bsp {

/// True while the device is in "deep idle": the energy task has decided the MCU may light-sleep (screen off, radios
/// off, no USB host). Periodic tasks (input polling, LVGL) read it and slow down so the chip can actually sleep;
/// the input task clears it at the first sign of a user action. Lives in bsp so every component can see it.
inline std::atomic<bool> deep_idle{false};

} // namespace bsp
