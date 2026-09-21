#include "imu/imu.hpp"

#include "bsp/board.hpp"
#include "bsp/pins.hpp"
#include "display/lvgl_port.hpp"
#include "settings/settings.hpp"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace imu {

namespace {

constexpr char TAG[] = "imu";

// QMI8658 register addresses -- the general-purpose/control ones
// (WHO_AM_I, CTRL1/2/7) are confirmed directly against the chip's own
// datasheet. The accelerometer DATA registers (AX_L..AZ_H) follow the
// standard QMI8658 layout used consistently across the reference
// drivers/libraries checked (Arduino, Rust, CircuitPython) -- not
// independently re-derived from the raw datasheet table here, since
// that table's exact byte offsets weren't fully legible in what was
// available. If accel readings come back obviously wrong (stuck at
// zero, or don't change at all when the board is moved), this is the
// first thing to double check against the full QMI8658 datasheet.
constexpr uint8_t REG_WHO_AM_I = 0x00;
constexpr uint8_t REG_CTRL1 = 0x02;
constexpr uint8_t REG_CTRL2 = 0x03; // accel: full-scale + ODR
constexpr uint8_t REG_CTRL7 = 0x08; // sensor enable (bit0 = aEN)
constexpr uint8_t REG_AX_L = 0x35;

constexpr uint8_t WHO_AM_I_EXPECTED = 0x05; // "identify the device is a QST sensor" per datasheet

// Best-effort default (matches common community QMI8658 init
// sequences) -- see REG_CTRL2's own comment above. Not critical to
// get the exact range/rate right for THIS feature (orientation
// sign-detection is robust to the scale being off), just needs SOME
// working accelerometer configuration.
constexpr uint8_t CTRL2_ACCEL_CONFIG = 0x23;

constexpr uint32_t I2C_FREQ_HZ = 100000;
constexpr uint8_t I2C_ADDR_HIGH = 0x6B; // SA0 high -- the default on this board per a Waveshare-specific report
constexpr uint8_t I2C_ADDR_LOW = 0x6A;  // SA0 low -- fallback

constexpr TickType_t AUTO_ROTATE_POLL_INTERVAL = pdMS_TO_TICKS(400);

// Accelerometer reading magnitude, roughly 1g at whatever range
// CTRL2_ACCEL_CONFIG selects -- used only as a "is this actually
// oriented one way or the other" confidence threshold, not a real
// physical unit conversion. If auto-rotate flips unreliably (flickers
// near the flip point, or never triggers), this is the value to
// adjust first.
constexpr int16_t FLIP_THRESHOLD = 4000;

i2c_master_bus_handle_t bus_handle = nullptr;
i2c_master_dev_handle_t dev_handle = nullptr;
bool present = false;

TaskHandle_t auto_rotate_task_handle = nullptr;
volatile bool auto_rotate_should_run = false;

bool read_register(uint8_t reg, uint8_t& out_value)
{
    return i2c_master_transmit_receive(dev_handle, &reg, 1, &out_value, 1, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool write_register(uint8_t reg, uint8_t value)
{
    const uint8_t payload[2] = {reg, value};
    return i2c_master_transmit(dev_handle, payload, sizeof(payload), pdMS_TO_TICKS(100)) == ESP_OK;
}

bool read_registers(uint8_t start_reg, uint8_t* out, size_t count)
{
    return i2c_master_transmit_receive(dev_handle, &start_reg, 1, out, count, pdMS_TO_TICKS(100)) == ESP_OK;
}

/**
 * @brief Try to talk to a QMI8658 at the given device handle -- reads
 *        WHO_AM_I and checks it matches. Used to test each
 *        I2C address during init() without committing to it first.
 */
bool probe(i2c_master_dev_handle_t handle)
{
    uint8_t who = 0;
    const uint8_t reg = REG_WHO_AM_I;
    if (i2c_master_transmit_receive(handle, &reg, 1, &who, 1, pdMS_TO_TICKS(100)) != ESP_OK) {
        return false;
    }
    return who == WHO_AM_I_EXPECTED;
}

bool try_device(uint8_t address)
{
    i2c_master_bus_config_t bus_config{};
    bus_config.i2c_port = -1; // auto-select a free port
    bus_config.sda_io_num = bsp::pins::IMU_SDA;
    bus_config.scl_io_num = bsp::pins::IMU_SCL;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t candidate_bus = nullptr;
    if (i2c_new_master_bus(&bus_config, &candidate_bus) != ESP_OK) {
        return false;
    }

    i2c_device_config_t dev_config{};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = address;
    dev_config.scl_speed_hz = I2C_FREQ_HZ;

    i2c_master_dev_handle_t candidate_dev = nullptr;
    if (i2c_master_bus_add_device(candidate_bus, &dev_config, &candidate_dev) != ESP_OK) {
        i2c_del_master_bus(candidate_bus);
        return false;
    }

    const bool found = probe(candidate_dev);
    if (found) {
        bus_handle = candidate_bus;
        dev_handle = candidate_dev;
        ESP_LOGI(TAG, "QMI8658 found: SDA=GPIO%d SCL=GPIO%d addr=0x%02X",
                 static_cast<int>(bsp::pins::IMU_SDA),
                 static_cast<int>(bsp::pins::IMU_SCL), address);
        return true;
    }

    i2c_master_bus_rm_device(candidate_dev);
    i2c_del_master_bus(candidate_bus);
    return false;
}

bool decide_flipped(int16_t x, int16_t y, int16_t z)
{
    // For the current board orientation, use the Y acceleration sign
    // to distinguish the two 180-degree landscape orientations.
    // This threshold is deliberately conservative to avoid flipping
    // while the device is close to level; it can be tuned from the
    // raw accel log if real hardware shows the opposite sign.
    (void)x;
    (void)z;
    return y < -FLIP_THRESHOLD;
}

void auto_rotate_task(void* /*arg*/)
{
    bool last_flipped = false;
    bool have_last = false;

    while (auto_rotate_should_run) {
        int16_t x = 0;
        int16_t y = 0;
        int16_t z = 0;
        if (read_accel(x, y, z)) {
            ESP_LOGD(TAG, "accel x=%d y=%d z=%d", x, y, z);
            const bool flipped = decide_flipped(x, y, z);
            if (!have_last || flipped != last_flipped) {
                lvgl_port::set_rotation(flipped);
                ESP_LOGI(TAG, "Auto orientation: %s", flipped ? "180" : "0");
                last_flipped = flipped;
                have_last = true;
            }
        }
        vTaskDelay(AUTO_ROTATE_POLL_INTERVAL);
    }

    auto_rotate_task_handle = nullptr;
    vTaskDelete(nullptr);
}

} // namespace

bool init()
{
    if (present) {
        return true;
    }

    // The Waveshare schematic explicitly maps IMU_SDA to GPIO48 and
    // IMU_SCL to GPIO47. GPIO46 is LCD_BL, so it must not be claimed by
    // the I2C bus. Try both common QMI8658 I2C addresses because the
    // SA0 strap is not needed to be hard-coded when probing is cheap.
    const uint8_t addresses[] = {I2C_ADDR_HIGH, I2C_ADDR_LOW};

    for (uint8_t address : addresses) {
        if (try_device(address)) {
            present = true;
            break;
        }
    }

    if (!present) {
        ESP_LOGW(TAG, "QMI8658 not found on GPIO%d(SDA)/GPIO%d(SCL) (tried both I2C addresses) -- auto-rotate unavailable",
                 static_cast<int>(bsp::pins::IMU_SDA),
                 static_cast<int>(bsp::pins::IMU_SCL));
        return false;
    }

    if (!write_register(REG_CTRL7, 0x01)) { // aEN=1, everything else disabled -- only the accelerometer is used
        ESP_LOGW(TAG, "Failed to enable QMI8658 accelerometer (CTRL7)");
    }
    if (!write_register(REG_CTRL2, CTRL2_ACCEL_CONFIG)) {
        ESP_LOGW(TAG, "Failed to configure QMI8658 accelerometer (CTRL2)");
    }
    (void)REG_CTRL1; // reserved for future use

    return true;
}

bool is_present()
{
    return present;
}

bool read_accel(int16_t& x, int16_t& y, int16_t& z)
{
    if (!present) {
        return false;
    }

    uint8_t raw[6];
    if (!read_registers(REG_AX_L, raw, sizeof(raw))) {
        return false;
    }

    x = static_cast<int16_t>(static_cast<uint16_t>(raw[0]) | (static_cast<uint16_t>(raw[1]) << 8));
    y = static_cast<int16_t>(static_cast<uint16_t>(raw[2]) | (static_cast<uint16_t>(raw[3]) << 8));
    z = static_cast<int16_t>(static_cast<uint16_t>(raw[4]) | (static_cast<uint16_t>(raw[5]) << 8));
    return true;
}

bool start_auto_rotate()
{
    if (!present) {
        return false;
    }
    if (auto_rotate_should_run) {
        return true; // already running
    }

    auto_rotate_should_run = true;
    const BaseType_t created =
        xTaskCreate(&auto_rotate_task, "imu_auto_rotate", 3072, nullptr, tskIDLE_PRIORITY + 1, &auto_rotate_task_handle);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create auto-rotate task");
        auto_rotate_should_run = false;
        return false;
    }
    return true;
}

void stop_auto_rotate()
{
    auto_rotate_should_run = false; // task exits on its own next poll cycle
}

bool is_auto_rotate_running()
{
    return auto_rotate_should_run;
}

} // namespace imu
