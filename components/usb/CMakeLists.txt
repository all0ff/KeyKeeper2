idf_component_register(
    SRCS
        "src/usb_service.cpp"
        "src/hid_keyboard.cpp"
        "src/type_engine.cpp"
        "src/keycode_map.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        vault
        security
)
