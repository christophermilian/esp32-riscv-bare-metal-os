//! SSD1306 OLED Display Driver
//! ============================
//!
//! Driver for 128x64 monochrome OLED displays using the SSD1306 controller.
//! Communicates via I²C interface.
//!
//! Features:
//! - Hardware I²C communication
//! - Full display buffer in RAM (1024 bytes)
//! - Text rendering with 5x7 font
//! - Basic graphics (pixels, rectangles)
//! - Display control (contrast, invert, on/off)

use crate::i2c;
use crate::font5x7::FONT5X7;

// Display dimensions
pub const SSD1306_WIDTH: usize = 128;
pub const SSD1306_HEIGHT: usize = 64;

// Common I2C addresses
pub const SSD1306_I2C_ADDR_DEFAULT: u8 = 0x3C;
pub const SSD1306_I2C_ADDR_ALT: u8 = 0x3D;

// SSD1306 Commands
const SSD1306_CMD_SET_CONTRAST: u8 = 0x81;
const SSD1306_CMD_DISPLAY_ALL_ON_RESUME: u8 = 0xA4;
const SSD1306_CMD_NORMAL_DISPLAY: u8 = 0xA6;
const SSD1306_CMD_INVERT_DISPLAY: u8 = 0xA7;
const SSD1306_CMD_DISPLAY_OFF: u8 = 0xAE;
const SSD1306_CMD_DISPLAY_ON: u8 = 0xAF;
const SSD1306_CMD_SET_DISPLAY_OFFSET: u8 = 0xD3;
const SSD1306_CMD_SET_START_LINE: u8 = 0x40;
const SSD1306_CMD_MEMORY_MODE: u8 = 0x20;
const SSD1306_CMD_COLUMN_ADDR: u8 = 0x21;
const SSD1306_CMD_PAGE_ADDR: u8 = 0x22;
const SSD1306_CMD_SET_COM_PINS: u8 = 0xDA;
const SSD1306_CMD_SET_DISPLAY_CLK_DIV: u8 = 0xD5;
const SSD1306_CMD_SET_PRECHARGE: u8 = 0xD9;
const SSD1306_CMD_SET_VCOM_DETECT: u8 = 0xDB;
const SSD1306_CMD_SET_MULTIPLEX: u8 = 0xA8;
const SSD1306_CMD_SEG_REMAP: u8 = 0xA0;
const SSD1306_CMD_COM_SCAN_DEC: u8 = 0xC8;
const SSD1306_CMD_CHARGE_PUMP: u8 = 0x8D;

// I²C Control Bytes
const SSD1306_CONTROL_CMD_SINGLE: u8 = 0x80;
const SSD1306_CONTROL_DATA_STREAM: u8 = 0x40;

// Display buffer (128x64 = 8192 bits = 1024 bytes)
static mut SSD1306_BUFFER: [u8; SSD1306_WIDTH * SSD1306_HEIGHT / 8] = [0; SSD1306_WIDTH * SSD1306_HEIGHT / 8];
static mut SSD1306_I2C_ADDR: u8 = 0;

/// SSD1306 configuration
pub struct Ssd1306Config {
    pub i2c_addr: u8,
    pub scl_pin: i32,
    pub sda_pin: i32,
}

// Send command to SSD1306
fn send_command(cmd: u8) -> bool {
    unsafe {
        let data = [SSD1306_CONTROL_CMD_SINGLE, cmd];
        i2c::write(SSD1306_I2C_ADDR, &data)
    }
}

// Send data to SSD1306
fn send_data(data: &[u8]) -> bool {
    unsafe {
        if !i2c::start() {
            return false;
        }

        // Write device address with write bit
        if !i2c::write_byte(SSD1306_I2C_ADDR << 1) {
            i2c::stop();
            return false;
        }

        // Write control byte for data stream
        if !i2c::write_byte(SSD1306_CONTROL_DATA_STREAM) {
            i2c::stop();
            return false;
        }

        // Write data bytes
        for &byte in data {
            if !i2c::write_byte(byte) {
                i2c::stop();
                return false;
            }
        }

        i2c::stop();
        true
    }
}

/// Initialize display
pub fn init(config: &Ssd1306Config) -> bool {
    unsafe {
        SSD1306_I2C_ADDR = config.i2c_addr;
    }

    // Initialize I²C peripheral
    let i2c_cfg = i2c::I2cConfig {
        scl_pin: config.scl_pin,
        sda_pin: config.sda_pin,
        freq_hz: 400000,  // 400kHz (fast mode I²C)
    };
    i2c::init(&i2c_cfg);

    // Power-up delay
    for _ in 0..100000 {
        core::hint::spin_loop();
    }

    // === SSD1306 Initialization Sequence ===
    send_command(SSD1306_CMD_DISPLAY_OFF);
    send_command(SSD1306_CMD_SET_DISPLAY_CLK_DIV);
    send_command(0x80);
    send_command(SSD1306_CMD_SET_MULTIPLEX);
    send_command((SSD1306_HEIGHT - 1) as u8);
    send_command(SSD1306_CMD_SET_DISPLAY_OFFSET);
    send_command(0x00);
    send_command(SSD1306_CMD_SET_START_LINE | 0x00);
    send_command(SSD1306_CMD_CHARGE_PUMP);
    send_command(0x14);
    send_command(SSD1306_CMD_MEMORY_MODE);
    send_command(0x00);
    send_command(SSD1306_CMD_SEG_REMAP | 0x01);
    send_command(SSD1306_CMD_COM_SCAN_DEC);
    send_command(SSD1306_CMD_SET_COM_PINS);
    send_command(0x12);
    send_command(SSD1306_CMD_SET_CONTRAST);
    send_command(0xCF);
    send_command(SSD1306_CMD_SET_PRECHARGE);
    send_command(0xF1);
    send_command(SSD1306_CMD_SET_VCOM_DETECT);
    send_command(0x40);
    send_command(SSD1306_CMD_DISPLAY_ALL_ON_RESUME);
    send_command(SSD1306_CMD_NORMAL_DISPLAY);
    send_command(SSD1306_CMD_DISPLAY_ON);

    // Clear display buffer and show blank screen
    clear();
    display();

    true
}

/// Clear the display buffer (set all pixels to black)
pub fn clear() {
    unsafe {
        for byte in SSD1306_BUFFER.iter_mut() {
            *byte = 0;
        }
    }
}

/// Update the physical display with the current buffer contents
pub fn display() {
    send_command(SSD1306_CMD_COLUMN_ADDR);
    send_command(0);
    send_command((SSD1306_WIDTH - 1) as u8);
    send_command(SSD1306_CMD_PAGE_ADDR);
    send_command(0);
    send_command((SSD1306_HEIGHT / 8 - 1) as u8);

    unsafe {
        send_data(&SSD1306_BUFFER);
    }
}

/// Set a single pixel in the display buffer
pub fn set_pixel(x: i32, y: i32, color: u8) {
    if x < 0 || x >= SSD1306_WIDTH as i32 || y < 0 || y >= SSD1306_HEIGHT as i32 {
        return;
    }

    let x = x as usize;
    let y = y as usize;

    unsafe {
        if color != 0 {
            SSD1306_BUFFER[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y & 7);
        } else {
            SSD1306_BUFFER[x + (y / 8) * SSD1306_WIDTH] &= !(1 << (y & 7));
        }
    }
}

/// Draw a single character using the 5x7 font
pub fn draw_char(x: i32, y: i32, c: char) {
    let c = if c < ' ' || c > '~' { ' ' } else { c };
    let idx = (c as usize) - 32;

    if idx >= FONT5X7.len() {
        return;
    }

    let glyph = &FONT5X7[idx];

    for i in 0..5 {
        let line = glyph[i];
        for j in 0..7 {
            if (line & (1 << j)) != 0 {
                set_pixel(x + i as i32, y + j as i32, 1);
            }
        }
    }
}

/// Draw a text string
pub fn draw_string(x: i32, y: i32, s: &str) {
    let mut cursor_x = x;
    let mut cursor_y = y;

    for c in s.chars() {
        if c == '\n' {
            cursor_x = x;
            cursor_y += 8;
        } else {
            draw_char(cursor_x, cursor_y, c);
            cursor_x += 6;  // 5 pixels + 1 space

            if cursor_x >= SSD1306_WIDTH as i32 {
                cursor_x = x;
                cursor_y += 8;
            }
        }
    }
}

/// Draw a filled rectangle
pub fn fill_rect(x: i32, y: i32, w: i32, h: i32, color: u8) {
    for i in x..(x + w) {
        for j in y..(y + h) {
            set_pixel(i, j, color);
        }
    }
}

/// Set display brightness/contrast
pub fn set_contrast(contrast: u8) {
    send_command(SSD1306_CMD_SET_CONTRAST);
    send_command(contrast);
}

/// Turn display on or off
pub fn display_on(on: bool) {
    send_command(if on { SSD1306_CMD_DISPLAY_ON } else { SSD1306_CMD_DISPLAY_OFF });
}

/// Invert display colors
pub fn invert_display(invert: bool) {
    send_command(if invert { SSD1306_CMD_INVERT_DISPLAY } else { SSD1306_CMD_NORMAL_DISPLAY });
}
