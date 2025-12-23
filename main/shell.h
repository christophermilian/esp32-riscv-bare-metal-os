/*
 * Shell - Simple command-line interface for OLED display
 * Input: USB Serial (from laptop keyboard)
 * Output: SSD1306 OLED display
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include <stdbool.h>

// Shell configuration
#define SHELL_MAX_LINE_LENGTH 64
#define SHELL_MAX_ARGS 8
#define SHELL_HISTORY_SIZE 5

// Initialize shell system
void shell_init(void);

// Process incoming character from serial input
void shell_process_char(char c);

// Execute a complete command line
void shell_execute(const char *cmdline);

// Display shell prompt on OLED
void shell_refresh_display(void);

#endif // SHELL_H
