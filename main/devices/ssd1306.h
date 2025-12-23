#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>

// Display dimensions
#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64

// Common I2C addresses
#define SSD1306_I2C_ADDR_DEFAULT 0x3C
#define SSD1306_I2C_ADDR_ALT     0x3D

// SSD1306 configuration
typedef struct {
    uint8_t i2c_addr;
    int scl_pin;
    int sda_pin;
} ssd1306_config_t;

// Initialize display
bool ssd1306_init(const ssd1306_config_t *config);

// Clear display (fill with black)
void ssd1306_clear(void);

// Update display with buffer contents
void ssd1306_display(void);

// Set a pixel (x, y) to on (1) or off (0)
void ssd1306_set_pixel(int x, int y, uint8_t color);

// Draw character at position
void ssd1306_draw_char(int x, int y, char c);

// Draw string at position
void ssd1306_draw_string(int x, int y, const char *str);

// Draw filled rectangle
void ssd1306_fill_rect(int x, int y, int w, int h, uint8_t color);

// Set display contrast (0-255)
void ssd1306_set_contrast(uint8_t contrast);

// Turn display on/off
void ssd1306_display_on(bool on);

// Invert display colors
void ssd1306_invert_display(bool invert);

#endif // SSD1306_H
