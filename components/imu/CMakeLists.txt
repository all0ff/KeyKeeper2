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

    # esp_driver_i2c is this project's first I2C usage -- confirmed as
    # the correct, current (non-legacy) ESP-IDF v6.0.2 component name
    # for the i2c_master.h API (i2c_new_master_bus(),
    # i2c_master_bus_add_device(), ...) used here, matching this
    # project's own established pattern of precise per-peripheral
    # REQUIRES (esp_driver_gpio, esp_driver_spi, ...) rather than the
    # umbrella "driver" component.
)
