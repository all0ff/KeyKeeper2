idf_component_register(
    SRCS
        "src/display.cpp"
        "src/lvgl_port.cpp"

    INCLUDE_DIRS
        "include"

    PRIV_INCLUDE_DIRS
        "src"

    REQUIRES
        bsp
        driver
        esp_driver_gpio
        esp_driver_spi
        esp_timer
        log

    # LovyanGFX is not published on the ESP Component Registry and must
    # be vendored manually into components/LovyanGFX (see README in this
    # directory). lvgl is pulled automatically via idf_component.yml.
    PRIV_REQUIRES
        LovyanGFX
        lvgl
)
