cmake_minimum_required(VERSION 3.16)

# ============================================================
# KeyKeeper2
# ESP-IDF project
# Target: ESP32-S3
# Board: Waveshare ESP32-S3-LCD-1.47B
# ============================================================

# TEMPORARY: components/gui is a leftover skeleton directory from the
# original project archive. Its CMakeLists.txt references components
# that don't exist yet ("system"), so ESP-IDF's automatic component
# discovery (which picks up every directory under components/, whether
# or not main requires it) fails to configure.
#
# components/security and components/vault were in this list too
# until now -- both are properly implemented and no longer need
# excluding.
#
# Excluded here until gui is actually implemented per
# docs/BUILD_PLAN.md -- remove the entry below at that point instead
# of deleting this line wholesale.
set(EXCLUDE_COMPONENTS "gui")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(KeyKeeper2)
