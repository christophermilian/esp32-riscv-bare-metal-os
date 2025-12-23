idf_component_register(
    SRCS
        "main.c"
        "shell.c"
        "drivers/console.c"
        "drivers/gpio.c"
        "drivers/i2c.c"
        "devices/ssd1306.c"
    INCLUDE_DIRS
        "."
        "drivers"
        "devices"
        "assets"
        "startup"
)

# Note: We use ESP-IDF's standard initialization and linker scripts
# ESP-IDF bootloader -> call_start_cpu0 -> app_main (our bare-metal code)
# boot.S and linker.ld are available in startup/ for future full bare-metal customization
