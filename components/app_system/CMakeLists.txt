idf_component_register(
    SRCS
        "src/app_system.cpp"
        "src/system_state.cpp"
        "src/logger.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        display
        input
        power
        storage
        event_bus
        interfaces
        settings
        security
        vault
        ui
        esp_timer
        log
)