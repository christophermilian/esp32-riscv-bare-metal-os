/*
 * Minimal bootloader main function
 * Performs basic hardware initialization before jumping to main app
 */

#include <stdint.h>

// Simple UART output for bootloader debugging
#define UART0_FIFO_REG   0x60000000
#define UART0_STATUS_REG 0x6000001C
#define UART_TXFIFO_FULL (1 << 0)

static void bootloader_putc(char c) {
    volatile uint32_t *status = (volatile uint32_t *)UART0_STATUS_REG;
    volatile uint32_t *fifo = (volatile uint32_t *)UART0_FIFO_REG;

    // Wait for TX FIFO
    while (*status & UART_TXFIFO_FULL);
    *fifo = c;
}

static void bootloader_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            bootloader_putc('\r');
        }
        bootloader_putc(*s++);
    }
}

void bootloader_main(void) {
    // UART is already initialized by ROM bootloader at 115200
    // We can use it directly

    bootloader_puts("\n\n");
    bootloader_puts("========================================\n");
    bootloader_puts("Custom Bare-Metal Bootloader v1.0\n");
    bootloader_puts("ESP32-C3 RISC-V\n");
    bootloader_puts("========================================\n");
    bootloader_puts("Initializing hardware...\n");

    // TODO: Add any hardware initialization here
    // - Clock configuration
    // - Cache initialization
    // - etc.

    bootloader_puts("Loading application from flash...\n");
    bootloader_puts("Jumping to app at 0x42000020\n");
    bootloader_puts("========================================\n\n");

    // Return to assembly code which will jump to app
}
