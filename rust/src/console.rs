//! ESP32-C3 USB Serial/JTAG Console Driver
//! ========================================
//!
//! This driver provides console I/O over the ESP32-C3's built-in USB Serial/JTAG
//! peripheral. When you connect the board via USB-C, this peripheral handles
//! bidirectional serial communication without needing external USB-to-serial chips.
//!
//! How It Works:
//! -------------
//! 1. Characters are buffered in RAM (software buffer)
//! 2. When buffer is full or explicitly flushed, we write to hardware registers
//! 3. The USB Serial/JTAG hardware has an internal FIFO that queues bytes
//! 4. Setting the WR_DONE bit tells hardware to transmit over USB
//! 5. Data travels over USB-C cable to your computer
//! 6. Your terminal program displays the characters
//!
//! Memory-Mapped Registers:
//! ------------------------
//! The USB Serial/JTAG peripheral is controlled by writing to specific memory
//! addresses. These aren't RAM - they're "windows" into hardware. Writing to
//! these addresses directly controls the USB peripheral.

use core::ptr::{read_volatile, write_volatile};

// ============================================================================
// HARDWARE REGISTER DEFINITIONS
// ============================================================================

// Base address of USB Serial/JTAG peripheral in memory map
const USB_SERIAL_JTAG_BASE: u32 = 0x60043000;

// EP1_REG: Endpoint 1 Data Register (offset 0x0000)
const USB_SERIAL_JTAG_EP1_REG: u32 = USB_SERIAL_JTAG_BASE + 0x0000;

// EP1_CONF_REG: Endpoint 1 Configuration Register (offset 0x0004)
const USB_SERIAL_JTAG_EP1_CONF_REG: u32 = USB_SERIAL_JTAG_BASE + 0x0004;

// EP0_REG: Endpoint 0 Data Register (offset 0x0010)
const USB_SERIAL_JTAG_EP0_REG: u32 = USB_SERIAL_JTAG_BASE + 0x0010;

// INT_RAW_REG: Interrupt Raw Status Register (offset 0x0034)
const USB_SERIAL_JTAG_INT_RAW_REG: u32 = USB_SERIAL_JTAG_BASE + 0x0034;

// SERIAL_OUT_RECV_PKT bit in INT_RAW_REG (bit 1)
const USB_SERIAL_JTAG_SERIAL_OUT_RECV_PKT: u32 = 1 << 1;

// INT_CLR_REG: Interrupt Clear Register (offset 0x0038)
const USB_SERIAL_JTAG_INT_CLR_REG: u32 = USB_SERIAL_JTAG_BASE + 0x0038;

// WR_DONE bit in EP1_CONF_REG (bit 0)
const USB_SERIAL_JTAG_WR_DONE: u32 = 1 << 0;

// ============================================================================
// REGISTER ACCESS FUNCTIONS
// ============================================================================

#[inline(always)]
fn reg_read(addr: u32) -> u32 {
    unsafe { read_volatile(addr as *const u32) }
}

#[inline(always)]
fn reg_write(addr: u32, val: u32) {
    unsafe { write_volatile(addr as *mut u32, val) }
}

// ============================================================================
// SOFTWARE BUFFER
// ============================================================================

const BUFFER_SIZE: usize = 64;

static mut BUFFER: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];
static mut BUFFER_POS: usize = 0;

// ============================================================================
// PRIVATE HELPER FUNCTION
// ============================================================================

/// Send buffered characters to USB hardware
fn flush_buffer() {
    unsafe {
        if BUFFER_POS == 0 {
            return;
        }

        // Write all buffered characters to hardware FIFO
        for i in 0..BUFFER_POS {
            reg_write(USB_SERIAL_JTAG_EP1_REG, BUFFER[i] as u32);
        }

        // Tell hardware to transmit the FIFO contents over USB
        reg_write(USB_SERIAL_JTAG_EP1_CONF_REG, USB_SERIAL_JTAG_WR_DONE);

        // Reset our software buffer
        BUFFER_POS = 0;

        // Wait for USB hardware to begin transmission
        for _ in 0..10000 {
            core::hint::spin_loop();
        }
    }
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/// Initialize the console driver
pub fn init() {
    unsafe {
        BUFFER_POS = 0;
    }
    puts("console initialized successfully!\n");
}

/// Write a single character to console
pub fn putc(c: char) {
    unsafe {
        BUFFER[BUFFER_POS] = c as u8;
        BUFFER_POS += 1;

        if BUFFER_POS >= BUFFER_SIZE {
            flush_buffer();
        }
    }
}

/// Write a null-terminated string to console
pub fn puts(s: &str) {
    for c in s.chars() {
        if c == '\n' {
            putc('\r');
        }
        putc(c);
    }
    flush_buffer();
}

/// Read a single character from console (non-blocking)
/// Returns None if no character is available
pub fn getc() -> Option<char> {
    // Check if data is available
    let int_raw = reg_read(USB_SERIAL_JTAG_INT_RAW_REG);
    if (int_raw & USB_SERIAL_JTAG_SERIAL_OUT_RECV_PKT) != 0 {
        // Read character from EP0
        let c = reg_read(USB_SERIAL_JTAG_EP0_REG) as u8;
        
        // Clear the interrupt
        reg_write(USB_SERIAL_JTAG_INT_CLR_REG, USB_SERIAL_JTAG_SERIAL_OUT_RECV_PKT);
        
        return Some(c as char);
    }
    None
}
