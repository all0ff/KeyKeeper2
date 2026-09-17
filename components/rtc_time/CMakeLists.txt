idf_component_register(
    SRCS
        "src/rtc_time.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        esp_netif
        lwip
        esp_event
        log
)
