# Bootloader component
idf_component_register(
    SRCS "bootloader_start.S" "bootloader_main.c"
    INCLUDE_DIRS "."
)

# Use custom bootloader linker script
target_link_options(${COMPONENT_LIB} INTERFACE
    "-T${CMAKE_CURRENT_SOURCE_DIR}/bootloader.ld"
    "-Wl,--no-relax"
)
