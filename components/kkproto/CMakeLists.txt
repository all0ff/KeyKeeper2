# kkproto -- portable protocol library for the vault <-> dongle radio link.
# Everything except platform/psa/ is plain C++17 with no platform dependencies and is shared
# verbatim with the dongle firmware. Disabled by default (CONFIG_KEYKEEPER_KKPROTO), so it can
# never break a build that does not ask for it.
#
# NOTE: ESP-IDF reads REQUIRES / PRIV_REQUIRES in an early pass where CONFIG_* values are not
# available, so the dependency on mbedtls must be declared unconditionally. Only the source list
# depends on the option.
set(kkproto_srcs "")
if(CONFIG_KEYKEEPER_KKPROTO)
    set(kkproto_srcs
        "src/noise.cpp"
        "src/frames.cpp"
        "src/messages.cpp"
        "src/pairing.cpp"
        "src/selftest.cpp"
        "platform/psa/crypto_port_psa.cpp"
    )
endif()

idf_component_register(
    SRCS ${kkproto_srcs}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES mbedtls
)