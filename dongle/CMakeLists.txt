cmake_minimum_required(VERSION 3.16)

# KeyKeeper2 dongle firmware (Waveshare ESP32-S3-LCD-1.47B, wired to the vault over UART).
# The same project also builds the "vault simulator" (see sdkconfig.vaultsim and README.md): a stand-in
# for the vault, used to test the dongle until the real vault firmware speaks the link.
#
# Shared with the vault firmware, taken from ../components: kkproto (protocol), blelink (BLE transport),
# bsp (pins), display (LCD).
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components/kkproto"
    "${CMAKE_CURRENT_LIST_DIR}/../components/blelink"
    "${CMAKE_CURRENT_LIST_DIR}/../components/bsp"
    "${CMAKE_CURRENT_LIST_DIR}/../components/display")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kk_dongle)
