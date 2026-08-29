idf_component_register(
    SRCS
        "src/settings.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        storage
        event_bus
        log
)
