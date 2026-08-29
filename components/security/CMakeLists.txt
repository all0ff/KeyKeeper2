idf_component_register(
    SRCS
        "src/security_service.cpp"
        "src/pin_manager.cpp"
        "src/lock_manager.cpp"
        "src/session_manager.cpp"
        "src/permission_manager.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        storage
        settings
        event_bus
        input
        mbedtls
        esp_hw_support
        esp_timer
        log
)
