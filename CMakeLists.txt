# Bare-metal ESP32-C3 RISC-V OS Project
cmake_minimum_required(VERSION 3.16)

# Set bootloader components (use minimal ESP-IDF bootloader for now)
set(BOOTLOADER_BUILD_DIR "${CMAKE_BINARY_DIR}/bootloader")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Minimal build - only essential components
idf_build_set_property(MINIMAL_BUILD ON)

# Set extra components directories if you want custom bootloader
# set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/bootloader")

project(esp32-riscv-bare-metal-os)
