idf_component_register(
    SRCS
        "src/status/system_status.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        bsp
        display
        input
        power
        storage

    # channels/ (channel.hpp, input_channel.hpp, output_channel.hpp,
    # transport_types.hpp) is header-only by design -- pure abstract
    # contracts, no transport implemented yet. Nothing to add to SRCS
    # for it.
)
