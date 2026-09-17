idf_component_register(
    SRCS
        "src/totp.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        rtc_time
        mbedtls
        log
)
