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
        "src/screens/accounts_screen.cpp"
        "src/screens/vault_list_screen.cpp"
        "src/screens/account_view_screen.cpp"
        "src/screens/account_edit_screen.cpp"
        "src/screens/favorites_screen.cpp"
        "src/screens/categories_screen.cpp"
        "src/screens/search_screen.cpp"
        "src/screens/about_screen.cpp"
        "src/screens/backup_screen.cpp"
        "src/screens/settings_screen.cpp"
        "src/screens/general_settings_screen.cpp"
        "src/screens/security_settings_screen.cpp"
        "src/screens/password_gen_settings_screen.cpp"
        "src/screens/usb_settings_screen.cpp"
        "src/screens/system_info_screen.cpp"
        "src/screens/wifi_settings_screen.cpp"
        "src/widgets/keyboard.cpp"
        "src/widgets/text_entry.cpp"
        "src/async_pin_check.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        display
        input
        security
        password_gen
        settings
        storage
        vault
        usb
        wifi
        web
        power
        esp_app_format
        efuse
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
    # efuse is my best guess for esp_efuse_is_flash_encryption_enabled()
    # / esp_efuse.h (SystemInfoScreen's Flash Encryption status row) --
    # NOT yet verified against your ESP-IDF v6.0.2, unlike esp_app_format
    # above. If the build complains about a missing header/component
    # here, check the exact name in the error and swap it in.
    #
    # Still grows as more screens/widgets are added (see README.md for
    # the rest of docs/BUILD_PLAN.md's ui/ file list):
    #   event_bus -- once a screen that touches it exists.
)
