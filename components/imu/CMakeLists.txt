idf_component_register(
    SRCS
        "src/imu_disabled.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        display
        settings
        esp_driver_i2c
        esp_timer
        log

    # The real QMI8658 driver is kept in src/imu.cpp for later recovery.
    # It is intentionally not compiled right now because its experimental
    # GPIO48 I2C configuration conflicts with LCD_BL on this board.
)
