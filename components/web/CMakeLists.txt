set(web_srcs
    "src/web_service.cpp"
    "src/web_vault_routes.cpp"
    "src/web_settings_routes.cpp"
    "src/web_reorder_state.cpp"
    "src/web_reorder_routes.cpp"
)

# Lite build (see main/Kconfig.projbuild): no Web UI backup/restore.
# web_service.cpp has its own matching #if around the include and the
# register_backup_routes() call, and web_app_html.hpp compiles the
# Backup screen/button/help/JS out of the page itself -- all three
# have to agree. NOT also in the base list above.
if(NOT CONFIG_KEYKEEPER_LITE)
    list(APPEND web_srcs "src/web_backup_routes.cpp")
endif()

idf_component_register(
    SRCS
        ${web_srcs}

    INCLUDE_DIRS
        "include"

    REQUIRES
        security
        settings
        wifi
        vault
        display
        event_bus
        esp_http_server
        nvs_flash
        log

    # cJSON comes from espressif/cjson (see idf_component.yml) --
    # the built-in "json" component was REMOVED in ESP-IDF v6.0 (this
    # project's version); do not add "json" to REQUIRES, it no longer
    # exists. This was confirmed against the official v6.0.2 migration
    # notes, unlike some other component names in this project that
    # were only confirmed after a failed build.
)
