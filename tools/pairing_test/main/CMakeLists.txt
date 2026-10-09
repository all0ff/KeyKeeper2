idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
        "../../uart_link_test/main"   # link_frame.hpp: the same framing the wiring test verified
    REQUIRES
        kkproto
        esp_driver_uart
        esp_driver_gpio
        esp_timer
        esp_hw_support
)
