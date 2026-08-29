idf_component_register(
    SRCS
        "src/storage.cpp"
        "src/nvs_storage.cpp"
        "src/filesystem.cpp"
        "src/sdcard.cpp"
        "src/vaultfile.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        nvs_flash
        fatfs
        vfs
        esp_driver_sdmmc
        sdmmc
        log

    # joltwallet/littlefs, pulled via idf_component.yml. Component
    # target name assumed to be "littlefs" (registry package is
    # "joltwallet/littlefs") -- if the build can't find it, check the
    # actual name under managed_components/ and adjust here.
    PRIV_REQUIRES
        littlefs
)
