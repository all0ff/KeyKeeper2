idf_component_register(
    SRCS
        "app_main.cpp"
        "store.cpp"
        "ui.cpp"
        "usb_dev.cpp"
        # The typing code of the vault firmware is pure C++ (tested on a PC); compiled here directly,
        # without dragging in the whole usb component and its dependencies.
        "../../components/usb/src/typing_plan.cpp"
        "../../components/usb/src/typing_runner.cpp"
        "../../components/usb/src/keycode_map.cpp"
        "../../components/usb/src/cyrillic_layout.cpp"
        "../../components/usb/src/plan_wire.cpp"
        "../../components/usb/src/wire_executor.cpp"
        "../../components/usb/src/dongle_sink.cpp"
    INCLUDE_DIRS
        "."
        "../../tools/uart_link_test/main"   # link_frame.hpp: the UART framing verified in stage 3a
        "../../components/usb/include"
    REQUIRES
        kkproto
        bsp
        display
        lvgl
        nvs_flash
        esp_driver_uart
        esp_driver_gpio
        esp_timer
        esp_hw_support
)
