# wireless -- the vault's side of the radio-dongle link (see include/wireless/wireless.hpp).
#
# REQUIRES / PRIV_REQUIRES are read before CONFIG_* values exist, so they are unconditional; only the
# sources depend on CONFIG_KEYKEEPER_KKPROTO (kkproto and usb's wire files are built only with it).
if(CONFIG_KEYKEEPER_KKPROTO)
    set(wireless_srcs "src/wireless.cpp")
else()
    set(wireless_srcs "src/wireless_off.cpp")
endif()

idf_component_register(
    SRCS ${wireless_srcs}
    INCLUDE_DIRS "include"
    # link_frame.hpp (the UART framing) is shared with the dongle project and the wiring test.
    PRIV_INCLUDE_DIRS "../../tools/uart_link_test/main"
    REQUIRES usb
    PRIV_REQUIRES kkproto blelink bsp nvs_flash esp_driver_uart esp_driver_gpio esp_timer log
)
