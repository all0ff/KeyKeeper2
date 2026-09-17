idf_component_register(
    SRCS
        "src/wifi_service.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        settings
        event_bus
        rtc_time
        esp_wifi
        esp_netif
        esp_event
        log

    # esp_wifi/esp_netif/esp_event are standard, stable ESP-IDF
    # component names (unlike esp_app_format, which genuinely wasn't
    # obvious on this project's ESP-IDF v6.0.2) -- not separately
    # flagged for verification, but if the build disagrees, same fix
    # pattern as always: check the exact component name in the error.
)
