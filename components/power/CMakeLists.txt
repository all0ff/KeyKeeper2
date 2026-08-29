idf_component_register(
    SRCS
        "src/power.cpp"
        "src/power_manager.cpp"

    INCLUDE_DIRS
        "include"

    PRIV_INCLUDE_DIRS
        "src"

    REQUIRES
        bsp
        input
        driver
        esp_driver_gpio
        esp_hw_support
        esp_timer
        log
)
