idf_component_register(
    SRCS
        "src/display.cpp"
        "src/lvgl_port.cpp"
        "src/fonts/keykeeper_cyrillic_16.c"
        "src/fonts/keykeeper_cyrillic_18.c"
        "src/fonts/keykeeper_cyrillic_24.c"

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

    # LovyanGFX is not published on the ESP Component Registry, so it's
    # pulled as a Git dependency instead (pinned to a specific commit --
    # see idf_component.yml in this directory for why). lvgl is pulled
    # automatically too, from the registry.
    PRIV_REQUIRES
        LovyanGFX
        lvgl
)
