/*
* Bare-metal application entry point
* ESP-IDF bootloader does minimal initialization, then calls this
*
* ESP-IDF provides:
* - 2nd stage bootloader
* - Flash/cache configuration
* - Basic C runtime (.data, .bss, stack)
*
* We provide everything else (no FreeRTOS, no ESP-IDF libraries)
*/

#include <stdio.h>
#include "esp_task_wdt.h"
#include "console.h"
#include "gpio.h"
#include "ssd1306.h"
#include "shell.h"

void app_main(void) {
    // Disable watchdog FIRST
    esp_task_wdt_deinit();

    // Initialize console
    console_init();
    console_puts("\n\n=== BARE METAL OS BOOTING ===\n");

    // Initialize OLED display
    console_puts("Initializing OLED display...\n");
    ssd1306_config_t oled_config = {
        .i2c_addr = SSD1306_I2C_ADDR_DEFAULT,  // 0x3C (try 0x3D if this doesn't work)
        .scl_pin = 7,   // GPIO7 (SCL/D5 on XIAO ESP32-C3)
        .sda_pin = 6    // GPIO6 (SDA/D4 on XIAO ESP32-C3)
    };

    if (ssd1306_init(&oled_config)) {
        console_puts("OLED initialized successfully!\n");
    } else {
        console_puts("OLED initialization failed!\n");
        // Continue anyway - shell can work without OLED
    }

    // Initialize shell
    console_puts("Initializing shell...\n");
    shell_init();
    console_puts("\nShell ready! Type commands in your terminal.\n");
    console_puts("Commands will appear on the OLED display.\n\n");

    // Main loop - process serial input
    while(1) {
        // Check for incoming character from USB Serial
        int c = console_getc();
        if (c != -1) {
            // Process the character through the shell
            shell_process_char((char)c);
        }

        // Small delay to avoid busy-waiting
        for (volatile int i = 0; i < 100; i++);
    }
}
