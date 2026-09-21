#include "imu/imu.hpp"

#include "esp_log.h"

namespace imu {

namespace {
constexpr char TAG[] = "imu";
}

bool init()
{
    // Temporarily disabled while the QMI8658 wiring is verified.
    // The current experimental driver tries to claim GPIO48, which is
    // also LCD_BL on the Waveshare ESP32-S3-LCD-1.47B. Do not touch
    // the I2C pins until the actual IMU wiring is confirmed.
    ESP_LOGW(TAG, "QMI8658 driver disabled temporarily; auto-rotate unavailable");
    return false;
}

bool is_present()
{
    return false;
}

bool read_accel(int16_t& x, int16_t& y, int16_t& z)
{
    x = 0;
    y = 0;
    z = 0;
    return false;
}

bool start_auto_rotate()
{
    return false;
}

void stop_auto_rotate()
{
}

bool is_auto_rotate_running()
{
    return false;
}

} // namespace imu
