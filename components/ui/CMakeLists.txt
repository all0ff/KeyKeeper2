set(ui_srcs
    "src/ui.cpp"
    "src/ui_manager.cpp"
    "src/theme.cpp"
    "src/icon.cpp"
    "src/localization.cpp"
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
    "src/screens/settings_screen.cpp"
    "src/screens/general_settings_screen.cpp"
    "src/screens/security_settings_screen.cpp"
    "src/screens/usb_settings_screen.cpp"
    "src/screens/wireless_screen.cpp"
    "src/screens/system_info_screen.cpp"
    "src/screens/wifi_settings_screen.cpp"
    "src/widgets/keyboard.cpp"
    "src/widgets/text_entry.cpp"
    "src/async_pin_check.cpp"
)

set(ui_requires
    bsp
    display
    input
    security
    settings
    storage
    vault
    usb
    wireless
    wifi
    rtc_time
    totp
    imu
    web
    power
    esp_app_format
    efuse
    lvgl
    log
)

# Lite build (see main/Kconfig.projbuild): everything below is ONLY
# added to the build when NOT Lite -- it must not also appear in the
# base ui_srcs/ui_requires lists above, or the exclusion does nothing.
# (A first version of this block listed password_gen_settings_screen.cpp
# and password_gen in BOTH places: Full got them twice, harmlessly, but
# Lite still compiled and linked them anyway -- the only thing that
# actually disappeared there was the menu entry, not the code.)
#
# CONFIG_KEYKEEPER_LITE is an ESP-IDF Kconfig value, automatically
# available as a plain CMake variable here, no extra wiring needed.
# Every .cpp that references one of these screens has its own matching
# #if !CONFIG_KEYKEEPER_LITE around that reference (settings_screen.cpp,
# account_edit_screen.cpp, main_menu.cpp) -- both sides have to agree,
# or Lite either shows a dead entry (screen excluded, caller not) or
# fails to link (caller references a screen whose .cpp was never
# compiled).
if(NOT CONFIG_KEYKEEPER_LITE)
    # Password generator: its settings screen, and the component
    # itself (account_edit_screen.cpp's own generate action also uses
    # password_gen::generate(), so in Lite that call site has to be
    # compiled out too, not just this screen).
    list(APPEND ui_srcs "src/screens/password_gen_settings_screen.cpp")
    list(APPEND ui_requires password_gen)

    # Backup screen: Create/Restore Backup, CSV Export/Import, SD card
    # Refresh/Format -- one screen, opened from one main-menu entry.
    list(APPEND ui_srcs "src/screens/backup_screen.cpp")
endif()

idf_component_register(
    SRCS
        ${ui_srcs}

    INCLUDE_DIRS
        "include"

    REQUIRES
        ${ui_requires}

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
