idf_component_register(
    SRCS
        "src/energy.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        display
        input
        settings
        usb
        web
        wifi
        wireless
        driver
        esp_driver_gpio
        esp_adc
        esp_pm
        esp_timer
        freertos
        log
)
