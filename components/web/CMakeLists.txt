idf_component_register(
    SRCS
        "src/web_service.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        security
        settings
        wifi
        event_bus
        esp_http_server
        log

    # cJSON comes from espressif/cjson (see idf_component.yml) --
    # the built-in "json" component was REMOVED in ESP-IDF v6.0 (this
    # project's version); do not add "json" to REQUIRES, it no longer
    # exists. This was confirmed against the official v6.0.2 migration
    # notes, unlike some other component names in this project that
    # were only confirmed after a failed build.
)
