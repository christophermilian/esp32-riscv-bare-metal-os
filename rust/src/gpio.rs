//! ESP32-C3 GPIO Driver
//! Direct register access for GPIO control

use core::ptr::{read_volatile, write_volatile};

// GPIO register base addresses
const GPIO_BASE: u32 = 0x60004000;
const IO_MUX_BASE: u32 = 0x60009000;

// GPIO registers
const GPIO_ENABLE_REG: u32 = GPIO_BASE + 0x0020;
const GPIO_OUT_REG: u32 = GPIO_BASE + 0x0004;
const GPIO_OUT_W1TS_REG: u32 = GPIO_BASE + 0x0008;  // Write 1 to set
const GPIO_OUT_W1TC_REG: u32 = GPIO_BASE + 0x000C;  // Write 1 to clear

// IO MUX registers (one per GPIO)
#[inline(always)]
fn gpio_pin_mux_reg(n: u32) -> u32 {
    IO_MUX_BASE + 0x0004 + (n * 4)
}

// IO MUX configuration bits
const FUN_IE: u32 = 1 << 9;           // Input enable
const FUN_DRV_SHIFT: u32 = 10;        // Drive strength
const MCU_SEL_SHIFT: u32 = 12;        // Function select

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

/// Configure a GPIO pin as output
pub fn set_output(gpio_num: i32) {
    if gpio_num < 0 || gpio_num > 21 {
        return;  // ESP32-C3 has GPIO 0-21
    }
    
    let gpio_num = gpio_num as u32;

    // Configure IO MUX for GPIO function
    let mux_reg = gpio_pin_mux_reg(gpio_num);
    let mut mux_val = reg_read(mux_reg);

    // Set function to GPIO (function 1)
    mux_val &= !(0x7 << MCU_SEL_SHIFT);  // Clear function bits
    mux_val |= 1 << MCU_SEL_SHIFT;       // Set to GPIO function

    // Set drive strength to medium (2)
    mux_val &= !(0x3 << FUN_DRV_SHIFT);
    mux_val |= 2 << FUN_DRV_SHIFT;

    reg_write(mux_reg, mux_val);

    // Enable output
    reg_set_bit(GPIO_ENABLE_REG, 1 << gpio_num);
}

/// Set GPIO pin high
pub fn set_high(gpio_num: i32) {
    if gpio_num < 0 || gpio_num > 21 {
        return;
    }
    reg_write(GPIO_OUT_W1TS_REG, 1 << gpio_num);
}

/// Set GPIO pin low
pub fn set_low(gpio_num: i32) {
    if gpio_num < 0 || gpio_num > 21 {
        return;
    }
    reg_write(GPIO_OUT_W1TC_REG, 1 << gpio_num);
}

/// Toggle GPIO pin
pub fn toggle(gpio_num: i32) {
    if gpio_num < 0 || gpio_num > 21 {
        return;
    }
    let current = reg_read(GPIO_OUT_REG);
    if (current & (1 << gpio_num)) != 0 {
        set_low(gpio_num);
    } else {
        set_high(gpio_num);
    }
}
