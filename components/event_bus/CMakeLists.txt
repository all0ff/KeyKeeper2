idf_component_register(
    SRCS
        "src/event_bus.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        esp_timer
        log

    # Deliberately NOT requiring bsp/display/input/power/storage/vault
    # -- event_bus is generic and must not depend on the components
    # that will eventually publish onto it. See event.hpp.
)
