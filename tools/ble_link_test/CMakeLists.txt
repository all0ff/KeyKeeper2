cmake_minimum_required(VERSION 3.16)

# Standalone ESP-IDF project: a BLE link test between two boards (same hardware, two roles).
# It is NOT part of the KeyKeeper2 firmware. See README.md for the build commands
# (two builds from one source: "peripheral" and "central").

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ble_link_test)
