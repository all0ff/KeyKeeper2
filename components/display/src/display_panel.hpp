#pragma once

// =============================================================================
// display::internal
//
// PRIVATE header. Only display.cpp and lvgl_port.cpp (same component) may
// include this file. It exposes the raw LovyanGFX panel object so
// lvgl_port.cpp can push pixels in its flush callback, without leaking
// LovyanGFX types into the component's public API (display.hpp).
// =============================================================================

#include <LovyanGFX.hpp>

namespace display::internal {

/**
 * @brief LovyanGFX device for the board's ST7789 panel over SPI3.
 *
 * Construction only configures the bus/panel/light descriptors; no SPI
 * transaction happens until lcd().init() is called by display::init().
 */
class Panel : public lgfx::LGFX_Device
{
public:
    Panel();

private:
    lgfx::Panel_ST7789 panel_instance_;
    lgfx::Bus_SPI bus_instance_;
    lgfx::Light_PWM light_instance_;
};

/**
 * @brief Return the single Panel instance owned by the display component.
 *
 * Valid to call at any time; the object exists for the lifetime of the
 * program. Whether it has been init()-ed is tracked separately by
 * display::is_initialized().
 */
Panel& lcd();

} // namespace display::internal
