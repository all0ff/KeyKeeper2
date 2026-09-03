idf_component_register(
    SRCS
        "src/ui.cpp"
        "src/ui_manager.cpp"
        "src/theme.cpp"
        "src/icon.cpp"
        "src/screens/quick_screen.cpp"
        "src/screens/lock_screen.cpp"
        "src/screens/setup_pin_screen.cpp"
        "src/screens/main_menu.cpp"
        "src/screens/vault_list_screen.cpp"
        "src/screens/account_view_screen.cpp"
        "src/screens/account_edit_screen.cpp"
        "src/screens/settings_screen.cpp"
        "src/screens/general_settings_screen.cpp"
        "src/screens/security_settings_screen.cpp"
        "src/screens/usb_settings_screen.cpp"
        "src/screens/system_info_screen.cpp"
        "src/widgets/keyboard.cpp"
        "src/widgets/text_entry.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        display
        input
        security
        settings
        storage
        vault
        esp_app_format
        lvgl
        log

    # lvgl is public REQUIRES (not PRIV_REQUIRES) because screen.hpp
    # and ui_manager.hpp -- both public headers -- use lv_obj_t
    # directly, so any component that includes them also needs lvgl's
    # include path.
    #
    # esp_app_format confirmed as the correct component for
    # esp_app_desc.h / esp_app_get_description() on ESP-IDF v6.0.2 (was
    # missing, causing a "No such file" build error -- fixed here).
    # esp_get_free_heap_size() has been fine coming in transitively so
    # far; if that changes, same fix pattern applies -- check the
    # exact component name in the error and add it here.
    #
    # Still grows as more screens/widgets are added (see README.md for
    # the rest of docs/BUILD_PLAN.md's ui/ file list):
    #   event_bus -- once a screen that touches it exists.
)
