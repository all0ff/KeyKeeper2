# kkproto -- portable protocol library for the vault <-> dongle radio link.
# Everything except platform/psa/ is plain C++17 with no platform dependencies and is shared
# verbatim with the dongle firmware. Disabled by default (CONFIG_KEYKEEPER_KKPROTO), so it can
# never break a build that does not ask for it.
if(CONFIG_KEYKEEPER_KKPROTO)
    idf_component_register(
        SRCS
            "src/noise.cpp"
            "src/frames.cpp"
            "src/messages.cpp"
            "src/pairing.cpp"
            "src/selftest.cpp"
            "platform/psa/crypto_port_psa.cpp"
        INCLUDE_DIRS "include"
        PRIV_REQUIRES mbedtls
    )
else()
    idf_component_register(INCLUDE_DIRS "include")
endif()
