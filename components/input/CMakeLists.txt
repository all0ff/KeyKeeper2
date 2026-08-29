idf_component_register(
    SRCS
        "src/encoder.cpp"
        "src/button.cpp"
        "src/input.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        driver
        esp_driver_gpio
        esp_driver_pcnt
        esp_timer
        log
)
