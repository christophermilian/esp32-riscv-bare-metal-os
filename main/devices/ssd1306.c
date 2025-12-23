/*
* SSD1306 OLED Display Driver
* ============================
* 
* Driver for 128x64 monochrome OLED displays using the SSD1306 controller.
* Communicates via I²C interface.
* 
* Features:
* - Hardware I²C communication
* - Full display buffer in RAM (1024 bytes)
* - Text rendering with 5x7 font
* - Basic graphics (pixels, rectangles)
* - Display control (contrast, invert, on/off)
* 
* Memory Layout:
* Display is organized as 8 pages of 128 columns
* Each page is 8 pixels tall (1 byte = 8 vertical pixels)
* 
*     Page 0: Rows 0-7
*     Page 1: Rows 8-15
*     ...
*     Page 7: Rows 56-63
* 
* Based on SSD1306 datasheet rev 1.1
*/

#include "ssd1306.h"
#include "i2c.h"
#include "font5x7.h"
#include <string.h>

// SSD1306 Commands
// Display Control Commands
#define SSD1306_CMD_SET_CONTRAST            0x81  // Set contrast (0x00-0xFF)
#define SSD1306_CMD_DISPLAY_ALL_ON_RESUME   0xA4  // Resume from display all ON
#define SSD1306_CMD_DISPLAY_ALL_ON          0xA5  // Force all pixels ON (ignore RAM)
#define SSD1306_CMD_NORMAL_DISPLAY          0xA6  // Normal display (1=lit pixel)
#define SSD1306_CMD_INVERT_DISPLAY          0xA7  // Inverted display (0=lit pixel)
#define SSD1306_CMD_DISPLAY_OFF             0xAE  // Display OFF (sleep mode)
#define SSD1306_CMD_DISPLAY_ON              0xAF  // Display ON

// Addressing Commands
#define SSD1306_CMD_SET_DISPLAY_OFFSET      0xD3  // Set vertical display offset
#define SSD1306_CMD_SET_START_LINE          0x40  // Set display start line (0x40-0x7F)
#define SSD1306_CMD_MEMORY_MODE             0x20  // Set memory addressing mode
#define SSD1306_CMD_COLUMN_ADDR             0x21  // Set column address range
#define SSD1306_CMD_PAGE_ADDR               0x22  // Set page address range
#define SSD1306_CMD_SET_LOW_COLUMN          0x00  // Set lower column start address
#define SSD1306_CMD_SET_HIGH_COLUMN         0x10  // Set higher column start address

// Hardware Configuration Commands
#define SSD1306_CMD_SET_COM_PINS            0xDA  // Set COM pins hardware config
#define SSD1306_CMD_SET_DISPLAY_CLK_DIV     0xD5  // Set display clock divide ratio
#define SSD1306_CMD_SET_PRECHARGE           0xD9  // Set pre-charge period
#define SSD1306_CMD_SET_VCOM_DETECT         0xDB  // Set VCOMH deselect level
#define SSD1306_CMD_SET_MULTIPLEX           0xA8  // Set multiplex ratio (1-64)

// Orientation Commands
#define SSD1306_CMD_SEG_REMAP               0xA0  // Set segment re-map (0xA0/0xA1)
#define SSD1306_CMD_COM_SCAN_INC            0xC0  // Scan from COM0 to COM[N-1]
#define SSD1306_CMD_COM_SCAN_DEC            0xC8  // Scan from COM[N-1] to COM0

// Power Commands
#define SSD1306_CMD_CHARGE_PUMP             0x8D  // Enable/disable charge pump
#define SSD1306_CMD_EXTERNAL_VCC            0x01  // External VCC mode
#define SSD1306_CMD_SWITCH_CAP_VCC          0x02  // Switched capacitor VCC mode

// I²C Control Bytes (determines command vs data)
#define SSD1306_CONTROL_CMD_SINGLE  0x80  // Single command byte follows
#define SSD1306_CONTROL_CMD_STREAM  0x00  // Stream of command bytes follows
#define SSD1306_CONTROL_DATA_STREAM 0x40  // Stream of data bytes follows

// Display buffer (128x64 = 8192 bits = 1024 bytes)
static uint8_t ssd1306_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
static uint8_t ssd1306_i2c_addr;

// Send command to SSD1306
static bool ssd1306_send_command(uint8_t cmd) {
    uint8_t data[2] = {SSD1306_CONTROL_CMD_SINGLE, cmd};
    return i2c_write(ssd1306_i2c_addr, data, 2);
}

// Send data to SSD1306
// Note: Uses low-level I2C for efficiency (sends control byte + up to 1024 bytes in one transaction)
static bool ssd1306_send_data(const uint8_t *data, uint32_t len) {
    if (!i2c_start()) {
        return false;
    }

    // Write device address with write bit
    if (!i2c_write_byte(ssd1306_i2c_addr << 1)) {
        i2c_stop();
        return false;
    }

    // Write control byte for data stream
    if (!i2c_write_byte(SSD1306_CONTROL_DATA_STREAM)) {
        i2c_stop();
        return false;
    }

    // Write data bytes
    for (uint32_t i = 0; i < len; i++) {
        if (!i2c_write_byte(data[i])) {
            i2c_stop();
            return false;
        }
    }

    i2c_stop();
    return true;
}

bool ssd1306_init(const ssd1306_config_t *config) {
    ssd1306_i2c_addr = config->i2c_addr;

    // Initialize I²C peripheral
    i2c_config_t i2c_cfg = {
        .scl_pin = config->scl_pin,
        .sda_pin = config->sda_pin,
        .freq_hz = 400000  // 400kHz (fast mode I²C)
    };
    i2c_init(&i2c_cfg);

    // Power-up delay: Give display time to stabilize after power on
    for (volatile int i = 0; i < 100000; i++);

    // === SSD1306 Initialization Sequence ===
    // Based on datasheet recommended initialization
    
    // Turn display off during configuration
    ssd1306_send_command(SSD1306_CMD_DISPLAY_OFF);

    // Set display clock divide ratio/oscillator frequency
    // Bits 3:0 = divide ratio (reset value = 0000b)
    // Bits 7:4 = oscillator frequency (reset value = 1000b)
    ssd1306_send_command(SSD1306_CMD_SET_DISPLAY_CLK_DIV);
    ssd1306_send_command(0x80);  // Default value: divide ratio=1, freq=8

    // Set multiplex ratio (number of display lines)
    ssd1306_send_command(SSD1306_CMD_SET_MULTIPLEX);
    ssd1306_send_command(SSD1306_HEIGHT - 1);  // 64 lines - 1 = 0x3F

    // Set display vertical offset (shift mapping of rows)
    ssd1306_send_command(SSD1306_CMD_SET_DISPLAY_OFFSET);
    ssd1306_send_command(0x00);  // No offset

    // Set display start line (first row to display)
    ssd1306_send_command(SSD1306_CMD_SET_START_LINE | 0x00);  // Start at line 0

    // Enable internal charge pump (required for displays without external VCC)
    ssd1306_send_command(SSD1306_CMD_CHARGE_PUMP);
    ssd1306_send_command(0x14);  // 0x14 = enable, 0x10 = disable

    // Set memory addressing mode
    ssd1306_send_command(SSD1306_CMD_MEMORY_MODE);
    ssd1306_send_command(0x00);  // Horizontal addressing mode (auto-increment)

    // Set segment re-map (flip horizontally)
    ssd1306_send_command(SSD1306_CMD_SEG_REMAP | 0x01);  // Column 127 mapped to SEG0

    // Set COM output scan direction (flip vertically)
    ssd1306_send_command(SSD1306_CMD_COM_SCAN_DEC);  // Scan from COM[N-1] to COM0

    // Set COM pins hardware configuration
    ssd1306_send_command(SSD1306_CMD_SET_COM_PINS);
    ssd1306_send_command(0x12);  // Alternative COM pin config, disable COM L/R remap

    // Set contrast level (brightness)
    ssd1306_send_command(SSD1306_CMD_SET_CONTRAST);
    ssd1306_send_command(0xCF);  // Max brightness (0x00-0xFF)

    // Set pre-charge period
    ssd1306_send_command(SSD1306_CMD_SET_PRECHARGE);
    ssd1306_send_command(0xF1);  // Phase 1: 1 DCLK, Phase 2: 15 DCLKs

    // Set VCOMH deselect level
    ssd1306_send_command(SSD1306_CMD_SET_VCOM_DETECT);
    ssd1306_send_command(0x40);  // ~0.77 x VCC

    // Resume display from RAM content (don't force all pixels ON)
    ssd1306_send_command(SSD1306_CMD_DISPLAY_ALL_ON_RESUME);
    
    // Normal display mode (not inverted)
    ssd1306_send_command(SSD1306_CMD_NORMAL_DISPLAY);

    // Turn display on
    ssd1306_send_command(SSD1306_CMD_DISPLAY_ON);

    // Clear display buffer and show blank screen
    ssd1306_clear();
    ssd1306_display();

    return true;
}

// Clear the display buffer (set all pixels to black)
// Note: Call ssd1306_display() to update the physical screen
void ssd1306_clear(void) {
    memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
}

// Update the physical display with the current buffer contents
// Sends entire 1024-byte buffer to the display via I²C
void ssd1306_display(void) {
    // Set column address range
    ssd1306_send_command(SSD1306_CMD_COLUMN_ADDR);
    ssd1306_send_command(0);   // Start column
    ssd1306_send_command(SSD1306_WIDTH - 1);  // End column

    // Set page address range
    ssd1306_send_command(SSD1306_CMD_PAGE_ADDR);
    ssd1306_send_command(0);   // Start page
    ssd1306_send_command((SSD1306_HEIGHT / 8) - 1);  // End page

    // Send buffer
    ssd1306_send_data(ssd1306_buffer, sizeof(ssd1306_buffer));
}

// Set a single pixel in the display buffer
// x: column (0-127), y: row (0-63), color: 1=white, 0=black
void ssd1306_set_pixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return;
    }
    
    /*
     * Display buffer organization:
     * - Buffer is organized in "pages" (8 rows = 1 page)
     * - Each byte represents 8 vertical pixels
     * 
     * Buffer index calculation:
     *   byte_index = x + (y / 8) * SSD1306_WIDTH
     *   bit_position = y & 7  (same as y % 8)
     * 
     * Example: Pixel at (10, 20)
     *   page = 20 / 8 = 2 (page 2 = rows 16-23)
     *   byte_index = 10 + 2 * 128 = 266
     *   bit = 20 % 8 = 4 (bit 4 in that byte)
     */

    if (color) {
        ssd1306_buffer[x + (y / 8) * SSD1306_WIDTH] |= (1 << (y & 7));
    } else {
        ssd1306_buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y & 7));
    }
}

// Draw a single character using the 5x7 font
// x, y: top-left corner position. Non-printable chars replaced with space
void ssd1306_draw_char(int x, int y, char c) {
    if (c < 32 || c > 126) {
        c = 32;  // Replace with space
    }

    const uint8_t *glyph = font5x7[c - 32];

    for (int i = 0; i < 5; i++) {
        uint8_t line = glyph[i];
        for (int j = 0; j < 7; j++) {
            if (line & (1 << j)) {
                ssd1306_set_pixel(x + i, y + j, 1);
            }
        }
    }
}

// Draw a text string with automatic line wrapping
// Supports '\n' for newlines. Each char is 6 pixels wide (5 + 1 spacing)
void ssd1306_draw_string(int x, int y, const char *str) {
    int cursor_x = x;

    while (*str) {
        if (*str == '\n') {
            cursor_x = x;
            y += 8;
        } else {
            ssd1306_draw_char(cursor_x, y, *str);
            cursor_x += 6;  // 5 pixels + 1 space

            if (cursor_x >= SSD1306_WIDTH) {
                cursor_x = x;
                y += 8;
            }
        }
        str++;
    }
}

// Draw a filled rectangle
// x, y: top-left corner, w: width, h: height, color: 1=white, 0=black
void ssd1306_fill_rect(int x, int y, int w, int h, uint8_t color) {
    for (int i = x; i < x + w; i++) {
        for (int j = y; j < y + h; j++) {
            ssd1306_set_pixel(i, j, color);
        }
    }
}

// Set display brightness/contrast
// contrast: 0 (dim) to 255 (bright)
void ssd1306_set_contrast(uint8_t contrast) {
    ssd1306_send_command(SSD1306_CMD_SET_CONTRAST);
    ssd1306_send_command(contrast);
}

// Turn display on or off (sleep mode)
// on: true=display on, false=display off (saves power)
void ssd1306_display_on(bool on) {
    ssd1306_send_command(on ? SSD1306_CMD_DISPLAY_ON : SSD1306_CMD_DISPLAY_OFF);
}

// Invert display colors
// invert: true=inverted (black on white), false=normal (white on black)
void ssd1306_invert_display(bool invert) {
    ssd1306_send_command(invert ? SSD1306_CMD_INVERT_DISPLAY : SSD1306_CMD_NORMAL_DISPLAY);
}
