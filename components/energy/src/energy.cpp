#include "energy/energy.hpp"

#include "bsp/pins.hpp"
#include "display/display.hpp"
#include "input/input.hpp"
#include "settings/settings.hpp"
#include "usb/usb_service.hpp"
#include "web/web_service.hpp"
#include "wifi/wifi_service.hpp"
#include "wireless/wireless.hpp"

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <algorithm>
#include <atomic>

namespace energy {

namespace {

constexpr char TAG[] = "energy";

constexpr gpio_num_t PIN_BAT_ADC = bsp::pins::HEADER_GPIO8;  // ADC1 channel 7
constexpr gpio_num_t PIN_CHRG = bsp::pins::HEADER_GPIO9;
constexpr int DIVIDER_RATIO = 2;           // 220k / 220k
constexpr uint32_t TICK_MS = 500;
constexpr uint32_t BATTERY_EVERY_MS = 5000;
constexpr uint32_t WEB_QUIET_FOR_LOW_CPU_MS = 60'000;

bool started = false;

std::atomic<bool> g_present{false};
std::atomic<uint8_t> g_percent{0};
std::atomic<uint16_t> g_mv{0};
std::atomic<bool> g_charging{false};
std::atomic<bool> g_usb_host{false};
std::atomic<bool> g_cpu_reduced{false};
std::atomic<bool> g_wifi_suspended{false};

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

// ---- battery ----------------------------------------------------------------------------------
adc_oneshot_unit_handle_t g_adc = nullptr;
adc_cali_handle_t g_cali = nullptr;
adc_channel_t g_chan = ADC_CHANNEL_0;
bool g_adc_ok = false;

void adc_setup()
{
    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(static_cast<int>(PIN_BAT_ADC), &unit, &g_chan) != ESP_OK || unit != ADC_UNIT_1) {
        ESP_LOGE(TAG, "GPIO%d is not an ADC1 pin", static_cast<int>(PIN_BAT_ADC));
        return;
    }
    adc_oneshot_unit_init_cfg_t ucfg = {};
    ucfg.unit_id = ADC_UNIT_1;
    ucfg.ulp_mode = ADC_ULP_MODE_DISABLE;
    if (adc_oneshot_new_unit(&ucfg, &g_adc) != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed");
        return;
    }
    adc_oneshot_chan_cfg_t ccfg = {};
    ccfg.atten = ADC_ATTEN_DB_12; // up to ~2.5 V on the pin; 4.2 V / 2 = 2.1 V
    ccfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_oneshot_config_channel(g_adc, g_chan, &ccfg) != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel config failed");
        return;
    }
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali = {};
    cali.unit_id = ADC_UNIT_1;
    cali.chan = g_chan;
    cali.atten = ADC_ATTEN_DB_12;
    cali.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&cali, &g_cali) != ESP_OK) {
        g_cali = nullptr;
        ESP_LOGW(TAG, "ADC calibration unavailable, using a rough conversion");
    }
#endif
    g_adc_ok = true;
}

// Resting voltage of a Li-Po/Li-ion cell -> percent. The board's 3.3 V regulator stops regulating at about
// 3.5 V, so that is where 0 % is.
uint8_t percent_from_mv(int mv)
{
    struct P { int mv; int pct; };
    static constexpr P table[] = {{3500, 0}, {3600, 6}, {3700, 18}, {3800, 40}, {3900, 62}, {4000, 78}, {4100, 90}, {4200, 100}};
    if (mv <= table[0].mv) return 0;
    if (mv >= table[7].mv) return 100;
    for (int i = 1; i < 8; ++i) {
        if (mv <= table[i].mv) {
            const int span = table[i].mv - table[i - 1].mv;
            return static_cast<uint8_t>(table[i - 1].pct +
                                        (mv - table[i - 1].mv) * (table[i].pct - table[i - 1].pct) / span);
        }
    }
    return 100;
}

int read_battery_mv()
{
    if (!g_adc_ok) return -1;
    int sum = 0;
    int n = 0;
    for (int i = 0; i < 16; ++i) {
        int raw = 0;
        if (adc_oneshot_read(g_adc, g_chan, &raw) == ESP_OK) {
            sum += raw;
            ++n;
        }
        vTaskDelay(1);
    }
    if (n == 0) return -1;
    const int raw = sum / n;
    int pin_mv = 0;
    if (g_cali != nullptr && adc_cali_raw_to_voltage(g_cali, raw, &pin_mv) == ESP_OK) {
        // ok
    } else {
        pin_mv = raw * 2500 / 4095; // rough, 12 dB attenuation
    }
    // Measured against a lab supply (4190 / 3910 / 3550 mV real -> 4238 / 3950 / 3588 mV read): the chain reads
    // about 1.5 % high (divider resistor tolerance plus ADC). Least-squares fit: real = 0.985 * read + 17 mV,
    // within 2 mV of all three points. For another board or other resistors measure the same way and adjust
    // these two numbers.
    constexpr int CAL_GAIN_PERMILLE = 985;
    constexpr int CAL_OFFSET_MV = 17;
    return (pin_mv * DIVIDER_RATIO * CAL_GAIN_PERMILLE) / 1000 + CAL_OFFSET_MV;
}

void update_battery()
{
    const int mv = read_battery_mv();
    if (mv >= 0) {
        // Smooth: the displayed value should not jump with every Wi-Fi burst.
        const int old = g_mv.load();
        const int smoothed = (old == 0) ? mv : (old * 3 + mv) / 4;
        g_mv = static_cast<uint16_t>(smoothed);
        g_present = smoothed >= 2500;
        g_percent = percent_from_mv(smoothed);
    }
    g_charging = gpio_get_level(PIN_CHRG) == 0;
}

// ---- CPU frequency --------------------------------------------------------------------------------
#if CONFIG_PM_ENABLE
esp_pm_lock_handle_t g_cpu_lock = nullptr;
bool g_lock_held = false;
bool g_pm_ok = false;
#endif

void pm_setup()
{
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm = {};
    pm.max_freq_mhz = 240;
    pm.min_freq_mhz = 80;
    pm.light_sleep_enable = false;
    if (esp_pm_configure(&pm) != ESP_OK) {
        ESP_LOGW(TAG, "esp_pm_configure failed -- CPU stays at its fixed frequency");
        return;
    }
    if (esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "kk_cpu", &g_cpu_lock) != ESP_OK) {
        ESP_LOGW(TAG, "esp_pm_lock_create failed");
        return;
    }
    esp_pm_lock_acquire(g_cpu_lock); // start at full speed
    g_lock_held = true;
    g_pm_ok = true;
    ESP_LOGI(TAG, "power management on: 80..240 MHz, no light sleep");
#else
    ESP_LOGW(TAG, "CONFIG_PM_ENABLE is off -- lowering the CPU frequency at idle is not available "
                  "(add CONFIG_PM_ENABLE=y to sdkconfig.defaults and recreate sdkconfig)");
#endif
}

void set_cpu_reduced(bool reduced)
{
#if CONFIG_PM_ENABLE
    if (!g_pm_ok || reduced == !g_lock_held) return;
    if (reduced) {
        esp_pm_lock_release(g_cpu_lock);
        g_lock_held = false;
    } else {
        esp_pm_lock_acquire(g_cpu_lock);
        g_lock_held = true;
    }
    g_cpu_reduced = reduced;
    ESP_LOGI(TAG, "CPU %s", reduced ? "reduced to 80 MHz" : "back to 240 MHz");
#else
    (void)reduced;
#endif
}

// ---- the task -------------------------------------------------------------------------------------------
void task(void*)
{
    uint32_t last_battery = 0;
    uint32_t wifi_suspended_at = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
        const uint32_t now = now_ms();
        const settings::PowerSettings& ps = settings::all().power;

        if (now - last_battery >= BATTERY_EVERY_MS || last_battery == 0) {
            last_battery = now;
            update_battery();
        }
        g_usb_host = usb::cable_connected();

        const uint32_t input_last = input::last_activity_ms();
        const uint32_t web_last = web::last_request_ms();
        const uint32_t use_last = std::max(input_last, web_last);
        const uint32_t idle_ms = now - use_last;
        const uint32_t input_idle_ms = now - input_last;

        // Wi-Fi: after the timeout the radio is stopped for now (wifi::stop(), the saved mode is NOT touched), and the
        // first use of the device after that brings it back with the saved settings.
        const settings::WifiSettings& w = settings::all().wifi;
        if (g_wifi_suspended.load()) {
            if (wifi::state() != wifi::ConnectionState::Idle) {
                g_wifi_suspended = false; // somebody (the Wi-Fi menu) started it again meanwhile
            } else if (ps.wifi_idle_off_min == 0 || w.mode == settings::WifiMode::Disabled) {
                // the timeout was switched off (or Wi-Fi was switched off by hand): restore, if Wi-Fi is wanted
                g_wifi_suspended = false;
                if (w.mode != settings::WifiMode::Disabled) wifi::apply_settings();
            } else if (input_last > wifi_suspended_at) {
                ESP_LOGI(TAG, "device used again -- Wi-Fi back on");
                g_wifi_suspended = false;
                wifi::apply_settings();
            }
        } else if (ps.wifi_idle_off_min > 0 && w.mode != settings::WifiMode::Disabled &&
                   idle_ms >= static_cast<uint32_t>(ps.wifi_idle_off_min) * 60'000u) {
            ESP_LOGI(TAG, "no use for %u min -- Wi-Fi off until the device is used again",
                     static_cast<unsigned>(ps.wifi_idle_off_min));
            wifi::stop();
            wifi_suspended_at = now;
            g_wifi_suspended = true;
        }

        // BLE link to the dongle.
        const wireless::Status ws = wireless::status();
        if (ps.ble_idle_off_min == 0) {
            if (ws.idle_paused) {
                wireless::set_idle_pause(false);
            }
        } else if (ws.enabled && ws.paired && !ws.idle_paused &&
                   (ws.phase == wireless::Phase::Linked || ws.phase == wireless::Phase::Connecting) &&
                   input_idle_ms >= static_cast<uint32_t>(ps.ble_idle_off_min) * 60'000u) {
            wireless::set_idle_pause(true);
        }

        // CPU frequency.
        const bool quiet = display::is_asleep() && (now - web_last >= WEB_QUIET_FOR_LOW_CPU_MS || web_last == 0);
        set_cpu_reduced(ps.cpu_idle_low && quiet);
        if (!ps.cpu_idle_low && g_cpu_reduced.load()) {
            set_cpu_reduced(false);
        }
    }
}

} // namespace

bool init()
{
    if (started) return true;
    started = true;

    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << PIN_CHRG;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);

    adc_setup();
    pm_setup();
    update_battery();
    ESP_LOGI(TAG, "battery %d mV (%u%%), present=%d charging=%d", static_cast<int>(g_mv.load()),
             static_cast<unsigned>(g_percent.load()), g_present.load() ? 1 : 0, g_charging.load() ? 1 : 0);

    if (xTaskCreate(&task, "kk_energy", 4096, nullptr, 2, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "could not start the energy task");
        return false;
    }
    return true;
}

BatteryStatus battery()
{
    BatteryStatus b;
    b.present = g_present.load();
    b.percent = g_percent.load();
    b.millivolts = g_mv.load();
    b.charging = g_charging.load();
    b.usb_host = g_usb_host.load();
    return b;
}

bool cpu_reduced()
{
    return g_cpu_reduced.load();
}

bool wifi_suspended()
{
    return g_wifi_suspended.load();
}

} // namespace energy
