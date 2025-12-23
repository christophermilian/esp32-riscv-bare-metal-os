/*
* Console I/O abstraction layer
* 
* Hardware: ESP32-C3 USB Serial/JTAG
* Note: This is NOT UART! It uses the built-in USB peripheral.
*/

#ifndef CONSOLE_H
#define CONSOLE_H

void console_init(void);
void console_putc(char c);
void console_puts(const char *s);
int console_getc(void);  // Returns -1 if no character available, otherwise the character

#endif
