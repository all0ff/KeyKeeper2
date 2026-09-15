idf_component_register(
    SRCS
        "src/password_gen.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        settings
        esp_hw_support
        log

    # esp_random()/esp_fill_random() live in esp_hw_support's
    # esp_random.h -- confirmed directly (this project's own
    # components/security already REQUIRES esp_hw_support for the
    # exact same header, used there for salt generation).
)
