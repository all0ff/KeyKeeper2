set(vault_srcs
    "src/vault.cpp"
    "src/vault_model.cpp"
    "src/vault_repository.cpp"
    "src/vault_wipe.cpp"
    "src/vault_order.cpp"
    "src/bip39_wordlist.cpp"
)

# Lite build (see main/Kconfig.projbuild): no on-device Backup screen
# and no Web UI backup routes, so nothing calls vault::backup:: or
# vault::csv:: -- both are self-contained (nothing else in this
# component includes either header; vault.cpp/vault_repository.cpp
# only mention them in comments), so excluding the two .cpp files is
# a real exclusion here, not just an unused-code one. Only in the
# NOT-Lite branch, never also in the base list above -- see
# components/ui/CMakeLists.txt's own comment on why.
if(NOT CONFIG_KEYKEEPER_LITE)
    list(APPEND vault_srcs
        "src/vault_backup.cpp"
        "src/vault_csv.cpp"
    )
endif()

idf_component_register(
    SRCS
        ${vault_srcs}

    INCLUDE_DIRS
        "include"

    REQUIRES
        storage
        security
        event_bus
        esp_timer
        esp_hw_support
        mbedtls
        nvs_flash
        log
)
