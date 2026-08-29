#include "display/display.hpp"

#include "display_panel.hpp"

#include "bsp/board.hpp"
#include "bsp/pins.hpp"
#include "display/display_config.hpp"

#include "esp_log.h"

namespace display {

namespace {

constexpr char TAG[] = "display";

bool initialized = false;
uint8_t current_brightness = 100;

/*
 * Logical (post-rotation) config exposed to the rest of the firmware.
 * offset_rotation = 1 (see internal::Panel()) swaps the native 172x320
 * portrait panel memory into a 320x172 landscape drawing surface, which
 * is the orientation this reference config was verified against.
 */
constexpr Config LOGICAL_CONFIG{
    .width = 320,
    .height = 172,
    .rotation = 1,
};

} // namespace

namespace internal {

Panel::Panel()
{
    // -------------------------------------------------------------------
    // SPI bus
    // -------------------------------------------------------------------
    {
        auto cfg = bus_instance_.config();

        cfg.spi_host = display::params::SpiBus::host;
        cfg.spi_mode = display::params::SpiBus::spi_mode;
        cfg.freq_write = display::params::SpiBus::freq_write_hz;
        cfg.freq_read = display::params::SpiBus::freq_read_hz;
        cfg.spi_3wire = display::params::SpiBus::spi_3wire;
        cfg.use_lock = true;
        cfg.dma_channel = SPI_DMA_CH_AUTO;

        cfg.pin_sclk = bsp::pins::LCD_SCLK;
        cfg.pin_mosi = bsp::pins::LCD_MOSI;
        cfg.pin_miso = -1; // not connected
        cfg.pin_dc = bsp::pins::LCD_DC;

        bus_instance_.config(cfg);
        panel_instance_.setBus(&bus_instance_);
    }

    // -------------------------------------------------------------------
    // Panel (ST7789, native 172x320 portrait memory)
    // -------------------------------------------------------------------
    {
        auto cfg = panel_instance_.config();

        const auto& lcd_info = bsp::board::info().lcd;

        cfg.pin_cs = bsp::pins::LCD_CS;
        cfg.pin_rst = bsp::pins::LCD_RST;
        cfg.pin_busy = -1; // not connected

        cfg.panel_width = lcd_info.width;   // 172 (native, unrotated)
        cfg.panel_height = lcd_info.height; // 320 (native, unrotated)

        cfg.offset_rotation = lcd_info.offset_rotation;
        cfg.offset_x = lcd_info.offset_x;
        cfg.offset_y = lcd_info.offset_y;

        cfg.invert = lcd_info.invert_color;
        cfg.rgb_order = lcd_info.rgb_order;

        cfg.readable = true;
        cfg.dlen_16bit = false;
        cfg.bus_shared = display::params::SpiBus::bus_shared;

        cfg.dummy_read_pixel = display::params::SpiBus::dummy_read_pixel_bits;
        cfg.dummy_read_bits = display::params::SpiBus::dummy_read_bits;

        panel_instance_.config(cfg);
    }

    // -------------------------------------------------------------------
    // Backlight (LEDC PWM)
    // -------------------------------------------------------------------
    {
        auto cfg = light_instance_.config();

        cfg.pin_bl = bsp::pins::LCD_BL;
        cfg.invert = display::params::Backlight::invert;
        cfg.freq = display::params::Backlight::pwm_freq_hz;
        cfg.pwm_channel = display::params::Backlight::pwm_channel;

        light_instance_.config(cfg);
        panel_instance_.setLight(&light_instance_);
    }

    setPanel(&panel_instance_);
}

Panel& lcd()
{
    static Panel instance;
    return instance;
}

} // namespace internal

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "display::init() called more than once, ignoring");
        return true;
    }

    ESP_LOGI(TAG, "Initializing display: %s", bsp::board::info().identity.name);
    ESP_LOGI(TAG, "Panel: %s, native %ux%u, logical %ux%u (rotation %u)",
             bsp::board::info().lcd.controller,
             static_cast<unsigned>(bsp::board::info().lcd.width),
             static_cast<unsigned>(bsp::board::info().lcd.height),
             static_cast<unsigned>(LOGICAL_CONFIG.width),
             static_cast<unsigned>(LOGICAL_CONFIG.height));

    if (!internal::lcd().init()) {
        ESP_LOGE(TAG, "LovyanGFX panel init() failed");
        return false;
    }

    /*
     * Keep the backlight off until the panel has been cleared, so the
     * user never sees stale/garbage VRAM content on power-up.
     */
    internal::lcd().setBrightness(0);
    internal::lcd().clear(0x0000);
    internal::lcd().display();

    initialized = true;
    set_brightness(current_brightness);

    ESP_LOGI(TAG, "Display initialized successfully");
    return true;
}

bool is_initialized()
{
    return initialized;
}

const Config& config()
{
    return LOGICAL_CONFIG;
}

void set_backlight(bool enabled)
{
    if (!initialized) {
        return;
    }

    internal::lcd().setBrightness(enabled ? current_brightness : 0);
}

void set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }

    current_brightness = percent;

    if (!initialized) {
        return;
    }

    // LovyanGFX Light_PWM expects 0-255.
    const uint8_t duty = static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255) / 100);
    internal::lcd().setBrightness(duty);
}

uint8_t brightness()
{
    return current_brightness;
}

} // namespace display
