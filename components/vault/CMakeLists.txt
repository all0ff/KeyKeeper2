idf_component_register(
    SRCS
        "src/vault.cpp"
        "src/vault_model.cpp"
        "src/vault_repository.cpp"
        "src/vault_wipe.cpp"
        "src/vault_backup.cpp"
        "src/vault_csv.cpp"
        "src/bip39_wordlist.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        storage
        security
        event_bus
        esp_timer
        esp_hw_support
        log
)
