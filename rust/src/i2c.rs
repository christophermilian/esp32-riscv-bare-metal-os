//! ESP32-C3 Bit-Banged I2C Driver
//! Software implementation of I2C master using GPIO pins

use core::ptr::{read_volatile, write_volatile};

// GPIO register base addresses
const GPIO_BASE: u32 = 0x60004000;
const IO_MUX_BASE: u32 = 0x60009000;

// GPIO registers
const GPIO_ENABLE_REG: u32 = GPIO_BASE + 0x0020;
const GPIO_OUT_W1TS_REG: u32 = GPIO_BASE + 0x0008;
const GPIO_OUT_W1TC_REG: u32 = GPIO_BASE + 0x000C;
const GPIO_IN_REG: u32 = GPIO_BASE + 0x003C;

// IO MUX registers
#[inline(always)]
fn gpio_pin_mux_reg(n: u32) -> u32 {
    IO_MUX_BASE + 0x0004 + (n * 4)
}

// IO MUX configuration bits
const FUN_IE: u32 = 1 << 9;
const FUN_DRV_SHIFT: u32 = 10;
const MCU_SEL_SHIFT: u32 = 12;
const FUN_WPU: u32 = 1 << 7;   // Weak pull-up
const FUN_WPD: u32 = 1 << 8;   // Weak pull-down

// Register access functions
#[inline(always)]
fn reg_write(addr: u32, val: u32) {
    unsafe { write_volatile(addr as *mut u32, val) }
}

#[inline(always)]
fn reg_read(addr: u32) -> u32 {
    unsafe { read_volatile(addr as *const u32) }
}

#[inline(always)]
fn reg_set_bit(addr: u32, bit: u32) {
    reg_write(addr, reg_read(addr) | bit);
}

// I2C state
static mut I2C_DELAY_CYCLES: u32 = 0;
static mut SCL_GPIO: i32 = 0;
static mut SDA_GPIO: i32 = 0;

/// I2C configuration
pub struct I2cConfig {
    pub scl_pin: i32,
    pub sda_pin: i32,
    pub freq_hz: u32,
}

// Delay function for I2C timing
fn i2c_delay() {
    unsafe {
        for _ in 0..I2C_DELAY_CYCLES {
            core::hint::spin_loop();
        }
    }
}

// Set pin as open-drain output with pull-up
fn gpio_set_opendrain(gpio_num: i32) {
    if gpio_num < 0 || gpio_num > 21 {
        return;
    }
    
    let gpio_num = gpio_num as u32;
    let mux_reg = gpio_pin_mux_reg(gpio_num);
    let mut mux_val = reg_read(mux_reg);

    // Set function to GPIO
    mux_val &= !(0x7 << MCU_SEL_SHIFT);
    mux_val |= 1 << MCU_SEL_SHIFT;

    // Enable input and pull-up
    mux_val |= FUN_IE | FUN_WPU;
    mux_val &= !FUN_WPD;

    // Set drive strength
    mux_val &= !(0x3 << FUN_DRV_SHIFT);
    mux_val |= 2 << FUN_DRV_SHIFT;

    reg_write(mux_reg, mux_val);

    // Enable as output
    reg_set_bit(GPIO_ENABLE_REG, 1 << gpio_num);
}

// Set SCL high (release line, pulled up by resistor)
fn scl_high() {
    unsafe {
        reg_write(GPIO_OUT_W1TS_REG, 1 << SCL_GPIO);
    }
    i2c_delay();
}

// Set SCL low
fn scl_low() {
    unsafe {
        reg_write(GPIO_OUT_W1TC_REG, 1 << SCL_GPIO);
    }
    i2c_delay();
}

// Set SDA high (release line)
fn sda_high() {
    unsafe {
        reg_write(GPIO_OUT_W1TS_REG, 1 << SDA_GPIO);
    }
    i2c_delay();
}

// Set SDA low
fn sda_low() {
    unsafe {
        reg_write(GPIO_OUT_W1TC_REG, 1 << SDA_GPIO);
    }
    i2c_delay();
}

// Read SDA state
fn sda_read() -> bool {
    unsafe {
        (reg_read(GPIO_IN_REG) & (1 << SDA_GPIO)) != 0
    }
}

/// Initialize I2C master
pub fn init(config: &I2cConfig) {
    unsafe {
        SCL_GPIO = config.scl_pin;
        SDA_GPIO = config.sda_pin;

        // Calculate delay cycles for desired frequency
        // ESP32-C3 runs at 160MHz, adjust for desired I2C frequency
        I2C_DELAY_CYCLES = (160_000_000 / config.freq_hz) / 4;

        // Configure pins as open-drain
        gpio_set_opendrain(SCL_GPIO);
        gpio_set_opendrain(SDA_GPIO);

        // Initialize both lines high
        sda_high();
        scl_high();
    }
}

/// I2C start condition
pub fn start() -> bool {
    // SDA goes low while SCL is high
    sda_high();
    scl_high();
    sda_low();
    scl_low();
    true
}

/// I2C stop condition
pub fn stop() {
    // SDA goes high while SCL is high
    sda_low();
    scl_high();
    sda_high();
}

/// Write a byte to I2C bus (returns true if ACK received)
pub fn write_byte(data: u8) -> bool {
    // Write 8 bits
    for i in (0..8).rev() {
        if (data & (1 << i)) != 0 {
            sda_high();
        } else {
            sda_low();
        }
        scl_high();
        scl_low();
    }

    // Read ACK bit
    sda_high();  // Release SDA
    scl_high();
    let ack = !sda_read();  // ACK is active low
    scl_low();

    ack
}

/// Read a byte from I2C bus
pub fn read_byte(ack: bool) -> u8 {
    let mut data: u8 = 0;

    sda_high();  // Release SDA for reading

    // Read 8 bits
    for i in (0..8).rev() {
        scl_high();
        if sda_read() {
            data |= 1 << i;
        }
        scl_low();
    }

    // Send ACK/NACK
    if ack {
        sda_low();
    } else {
        sda_high();
    }
    scl_high();
    scl_low();
    sda_high();  // Release SDA

    data
}

/// Write multiple bytes to device
pub fn write(addr: u8, data: &[u8]) -> bool {
    if !start() {
        return false;
    }

    // Write device address with write bit
    if !write_byte(addr << 1) {
        stop();
        return false;
    }

    // Write data bytes
    for &byte in data {
        if !write_byte(byte) {
            stop();
            return false;
        }
    }

    stop();
    true
}

/// Write register data
pub fn write_reg(addr: u8, reg: u8, data: &[u8]) -> bool {
    if !start() {
        return false;
    }

    // Write device address with write bit
    if !write_byte(addr << 1) {
        stop();
        return false;
    }

    // Write register address
    if !write_byte(reg) {
        stop();
        return false;
    }

    // Write data bytes
    for &byte in data {
        if !write_byte(byte) {
            stop();
            return false;
        }
    }

    stop();
    true
}
