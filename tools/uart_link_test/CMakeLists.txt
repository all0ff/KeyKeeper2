cmake_minimum_required(VERSION 3.16)

# Standalone ESP-IDF project: a wiring test for the UART link between two boards.
# It is NOT part of the KeyKeeper2 firmware and shares nothing with it except the idea.
# Build it from this directory:  idf.py set-target esp32s3  &&  idf.py build flash monitor

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(uart_link_test)
