idf_component_register(
    SRCS 
        "src/screen_manager.cpp"
        "src/boot_screen.cpp"
        "src/quick_screen.cpp"
        "src/unlock_screen.cpp"
        "src/main_menu_screen.cpp"
        "src/accounts_screen.cpp"
    INCLUDE_DIRS "include"
    REQUIRES display input system vault security
)