idf_component_register(
    SRCS
        "src/imu.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        display
        settings
        esp_driver_i2c
        esp_timer
        log

    # QMI8658 uses the board's dedicated IMU_SDA/IMU_SCL pins.
)
