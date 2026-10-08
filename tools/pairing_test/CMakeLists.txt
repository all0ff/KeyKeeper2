cmake_minimum_required(VERSION 3.16)

# Standalone ESP-IDF project: pairing over the UART link between two boards, using the kkproto library
# from the main repository. It is NOT part of the KeyKeeper2 firmware.
# Build from this directory:  idf.py set-target esp32s3  &&  idf.py build flash monitor

# kkproto is a component of the main project; borrow it from there.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components/kkproto")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pairing_test)
