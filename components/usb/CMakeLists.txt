# The radio-dongle path (plan -> wire events -> DongleSink) needs the kkproto message encoding, which is only
# compiled with CONFIG_KEYKEEPER_KKPROTO. REQUIRES kkproto itself is unconditional (see kkproto/CMakeLists.txt).
set(usb_wire_srcs "")
if(CONFIG_KEYKEEPER_KKPROTO)
    set(usb_wire_srcs
        "src/plan_wire.cpp"
        "src/wire_executor.cpp"
        "src/dongle_sink.cpp"
    )
endif()

idf_component_register(
    SRCS
        "src/usb_service.cpp"
        "src/hid_keyboard.cpp"
        "src/type_engine.cpp"
        "src/typing_plan.cpp"
        "src/typing_runner.cpp"
        "src/cable_sink.cpp"
        "src/keycode_map.cpp"
        "src/cyrillic_layout.cpp"
        ${usb_wire_srcs}
    INCLUDE_DIRS "include"
    REQUIRES
        vault
        security
        settings
        totp
        rtc_time
        kkproto
)
