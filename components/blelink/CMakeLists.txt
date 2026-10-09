# blelink -- BLE transport for the vault <-> dongle link (NimBLE). See README.md.
#
# Without CONFIG_BT_NIMBLE_ENABLED only a stub is built (start() fails), so other components may
# depend on blelink unconditionally. REQUIRES is read before CONFIG_* values exist, hence unconditional.
if(CONFIG_BT_NIMBLE_ENABLED)
    set(blelink_srcs "src/blelink.cpp")
else()
    set(blelink_srcs "src/blelink_off.cpp")
endif()

idf_component_register(
    SRCS ${blelink_srcs}
    INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES bt nvs_flash esp_timer log heap
)
