//! Bare-metal application entry point
//! ESP-IDF bootloader does minimal initialization, then calls this
//!
//! ESP-IDF provides:
//! - 2nd stage bootloader
//! - Flash/cache configuration
//! - Basic C runtime (.data, .bss, stack)
//!
//! We provide everything else (no FreeRTOS, no ESP-IDF libraries)

#![no_std]
#![no_main]

mod console;
mod gpio;
mod i2c;
mod ssd1306;
mod shell;
mod font5x7;

use esp_backtrace as _;
use esp_hal::prelude::*;

#[entry]
fn main() -> ! {
    // Initialize ESP-HAL
    esp_hal::init(esp_hal::Config::default());
    
    // Initialize console
    console::init();
    console::puts("\n\n=== BARE METAL OS BOOTING ===\n");

    // Initialize OLED display
    console::puts("Initializing OLED display...\n");
    let oled_config = ssd1306::Ssd1306Config {
        i2c_addr: ssd1306::SSD1306_I2C_ADDR_DEFAULT,  // 0x3C
        scl_pin: 7,   // GPIO7 (SCL/D5 on XIAO ESP32-C3)
        sda_pin: 6,   // GPIO6 (SDA/D4 on XIAO ESP32-C3)
    };

    if ssd1306::init(&oled_config) {
        console::puts("OLED initialized successfully!\n");
    } else {
        console::puts("OLED initialization failed!\n");
        // Continue anyway - shell can work without OLED
    }

    // Initialize shell
    console::puts("Initializing shell...\n");
    shell::init();
    console::puts("\nShell ready! Type commands in your terminal.\n");
    console::puts("Commands will appear on the OLED display.\n\n");

    // Main loop - process serial input
    loop {
        // Check for incoming character from USB Serial
        if let Some(c) = console::getc() {
            // Process the character through the shell
            shell::process_char(c);
        }

        // Small delay to avoid busy-waiting
        for _ in 0..100 {
            core::hint::spin_loop();
        }
    }
}
