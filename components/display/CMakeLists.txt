idf_component_register(
    SRCS
        "src/display.cpp"
        "src/lvgl_port.cpp"
        # keykeeper_cyrillic_16/18.c are DELIBERATELY OUT of the build
        # right now -- applying them caused a confirmed regression
        # (no text rendered anywhere after boot). Root cause not yet
        # understood; see lvgl_port.cpp's own revert comment. The two
        # .c files are still sitting in src/fonts/, just not compiled,
        # until this is actually debugged. Re-add them here once fixed.

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
