idf_component_register(
    SRCS
        "src/board.cpp"
        "src/bsp.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        driver
        esp_driver_gpio
        log
)
