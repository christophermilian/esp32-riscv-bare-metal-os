/*
* ESP32-C3 USB Serial/JTAG Console Driver
* ========================================
* 
* This driver provides console I/O over the ESP32-C3's built-in USB Serial/JTAG
* peripheral. When you connect the board via USB-C, this peripheral handles
* bidirectional serial communication without needing external USB-to-serial chips.
* 
* How It Works:
* -------------
* 1. Characters are buffered in RAM (software buffer)
* 2. When buffer is full or explicitly flushed, we write to hardware registers
* 3. The USB Serial/JTAG hardware has an internal FIFO that queues bytes
* 4. Setting the WR_DONE bit tells hardware to transmit over USB
* 5. Data travels over USB-C cable to your computer
* 6. Your terminal program displays the characters
* 
* Memory-Mapped Registers:
* ------------------------
* The USB Serial/JTAG peripheral is controlled by writing to specific memory
* addresses. These aren't RAM - they're "windows" into hardware. Writing to
* these addresses directly controls the USB peripheral.
*/

#include "console.h"
#include <stdint.h>
#include <stdio.h>  // For getchar()

// ============================================================================
// HARDWARE REGISTER DEFINITIONS
// ============================================================================

// Base address of USB Serial/JTAG peripheral in memory map
// This is where the hardware registers live in the ESP32-C3's address space
#define USB_SERIAL_JTAG_BASE    0x60043000

// EP1_REG: Endpoint 1 Data Register (offset 0x0000)
// Writing a byte here puts it in the hardware's transmit FIFO
// Each write adds one character to the queue of data to be sent over USB
#define USB_SERIAL_JTAG_EP1_REG         (USB_SERIAL_JTAG_BASE + 0x0000)

// EP1_CONF_REG: Endpoint 1 Configuration Register (offset 0x0004)
// Writing to this register controls the behavior of the transmit FIFO
// Most importantly, it has the WR_DONE bit to trigger transmission
#define USB_SERIAL_JTAG_EP1_CONF_REG    (USB_SERIAL_JTAG_BASE + 0x0004)

// CONF0_REG: General Configuration Register (offset 0x0008)
// Not currently used, but available for future configuration needs
#define USB_SERIAL_JTAG_CONF0_REG       (USB_SERIAL_JTAG_BASE + 0x0008)

// EP0_REG: Endpoint 0 Data Register (offset 0x0010)
// Reading from this register retrieves one byte from the receive FIFO
#define USB_SERIAL_JTAG_EP0_REG         (USB_SERIAL_JTAG_BASE + 0x0010)

// INT_RAW_REG: Interrupt Raw Status Register (offset 0x0034)
// Bit 1 (SERIAL_OUT_RECV_PKT) indicates if data is available to read
#define USB_SERIAL_JTAG_INT_RAW_REG     (USB_SERIAL_JTAG_BASE + 0x0034)

// SERIAL_OUT_RECV_PKT bit in INT_RAW_REG (bit 1)
// When this bit is set, there's data available in the receive FIFO
#define USB_SERIAL_JTAG_SERIAL_OUT_RECV_PKT (1 << 1)

// INT_CLR_REG: Interrupt Clear Register (offset 0x0038)
// Write 1 to clear interrupt bits
#define USB_SERIAL_JTAG_INT_CLR_REG     (USB_SERIAL_JTAG_BASE + 0x0038)

// EP0_CONF_REG: Endpoint 0 Configuration Register (offset 0x0014)
// Contains status about receive FIFO
#define USB_SERIAL_JTAG_EP0_CONF_REG    (USB_SERIAL_JTAG_BASE + 0x0014)

// WR_DONE bit in EP1_CONF_REG (bit 0)
// When you set this bit, it tells the USB hardware:
// "I'm done writing characters to the FIFO, please send them over USB now!"
// The hardware automatically clears this bit after starting transmission
#define USB_SERIAL_JTAG_WR_DONE         (1 << 0)

// ============================================================================
// REGISTER ACCESS MACROS
// ============================================================================

// Read from a memory-mapped hardware register
// The 'volatile' keyword is CRITICAL - it tells the compiler:
// "This memory location can change without my code changing it (hardware does it),
//  so don't optimize away reads, and always actually read from this address"
#define REG_READ(addr)          (*((volatile uint32_t *)(addr)))

// Write to a memory-mapped hardware register
// Again, 'volatile' tells compiler: "Always actually write to this address,
// don't optimize it away even if you think the value isn't used"
// Without 'volatile', the compiler might skip writes it thinks are unnecessary!
#define REG_WRITE(addr, val)    (*((volatile uint32_t *)(addr)) = (val))

// ============================================================================
// SOFTWARE BUFFER
// ============================================================================

// Size of our software buffer (in bytes)
// We collect characters here before sending them to hardware
// Larger buffer = fewer hardware operations = more efficient
// But also = more RAM used and longer delay before text appears
// 64 bytes is a good balance for console output
#define BUFFER_SIZE 64

// The buffer itself - storage for characters waiting to be sent
// 'static' means this variable is private to this file and persists between calls
// It lives in RAM at a fixed location (not on the stack)
static char buffer[BUFFER_SIZE];

// Current position in the buffer (how many characters are waiting)
// When this reaches BUFFER_SIZE, we must flush to hardware
// 'static' means it persists and is initialized to 0 at program start
static int buffer_pos = 0;

// ============================================================================
// PRIVATE HELPER FUNCTION
// ============================================================================

/*
 * flush_buffer() - Send buffered characters to USB hardware
 * 
 * This function transfers all characters from our software buffer (in RAM)
 * to the USB Serial/JTAG hardware FIFO, then triggers USB transmission.
 * 
 * The Process:
 * 1. Check if buffer has any characters (return early if empty)
 * 2. Write each buffered character to the EP1_REG hardware register
 *    - Each write puts one byte into the hardware's internal FIFO
 *    - The hardware FIFO can hold multiple bytes for transmission
 * 3. Set the WR_DONE bit to tell hardware "start sending these bytes over USB"
 * 4. Reset our software buffer position to 0 (buffer is now empty)
 * 5. Wait a bit for USB transmission to start (hardware needs time)
 * 
 * Why the delay?
 * The hardware needs time to package bytes into USB packets and begin
 * transmission. If we immediately write more data, we might overflow
 * the hardware FIFO. The delay ensures the hardware has time to start
 * draining its FIFO by actually sending data over the USB cable.
 */
static void flush_buffer(void) {
    // Nothing to send? Don't waste time doing hardware operations
    if (buffer_pos == 0) {
        return;
    }
    
    // STEP 1: Write all buffered characters to hardware FIFO
    // Each iteration writes one byte to the EP1_REG register
    // The hardware accumulates these in its internal FIFO
    for (int i = 0; i < buffer_pos; i++) {
        // Write one character to the USB hardware's transmit FIFO
        // This is memory-mapped I/O: writing to 0x60043000 doesn't go to RAM,
        // it goes directly to the USB Serial/JTAG peripheral hardware!
        REG_WRITE(USB_SERIAL_JTAG_EP1_REG, (uint32_t)buffer[i]);
    }
    
    // STEP 2: Tell hardware to transmit the FIFO contents over USB
    // Setting the WR_DONE bit (bit 0) in the configuration register
    // signals to the USB hardware: "I'm done writing, please send!"
    // The hardware will then:
    // - Package the FIFO contents into USB packets
    // - Send those packets over the USB-C cable to your computer
    // - Clear the WR_DONE bit automatically when done
    REG_WRITE(USB_SERIAL_JTAG_EP1_CONF_REG, USB_SERIAL_JTAG_WR_DONE);
    
    // STEP 3: Reset our software buffer
    // All characters have been handed off to hardware, so buffer is now empty
    buffer_pos = 0;
    
    // STEP 4: Wait for USB hardware to begin transmission
    // The hardware needs time to start processing the FIFO and sending USB packets
    // At 160MHz CPU clock, this loop takes roughly 62.5 microseconds
    // This prevents us from immediately overflowing the hardware FIFO with new data
    // Think of it like giving the postal truck time to drive away before
    // putting more letters in the mailbox
    for (volatile int i = 0; i < 10000; i++);
    // Note: 'volatile' prevents compiler from optimizing away this "do nothing" loop
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/*
 * console_init() - Initialize the console driver
 * 
 * Currently does minimal work because the ESP-IDF bootloader has already
 * initialized the USB Serial/JTAG peripheral for us. The bootloader used
 * this peripheral to print boot messages, so it's ready to use.
 * 
 * Future Enhancement Ideas:
 * - Disable ESP-IDF's USB Serial/JTAG driver (if reconfiguring)
 * - Set custom USB buffer sizes
 * - Configure USB timeouts
 * - Initialize receive (RX) functionality for reading input
 */
void console_init(void) {
    // USB Serial/JTAG is already initialized by ESP-IDF bootloader
    // The hardware is ready to use, so we just ensure our software state is clean
    
    // Reset buffer position to 0 (redundant since static variables are
    // already zero-initialized, but explicit for clarity)
    buffer_pos = 0;
    
    // Print a test message to confirm console is working
    console_puts("console initialized successfully!\n");
}

/*
 * console_putc() - Write a single character to console
 * 
 * This is the fundamental building block for all console output.
 * Characters are added to the software buffer for efficiency.
 * 
 * Buffering Strategy:
 * - Add character to buffer (fast - just a RAM write)
 * - If buffer is full, flush to hardware (slower - involves hardware I/O)
 * - This batches hardware operations for better performance
 * 
 * Why buffer instead of writing directly to hardware every time?
 * Hardware I/O is much slower than RAM operations. If we wrote each
 * character directly to USB hardware, we'd waste CPU cycles waiting.
 * By buffering, we can queue up characters in fast RAM, then send
 * them all to hardware at once. It's like putting multiple letters
 * in your mailbox before calling the postal truck, instead of calling
 * the truck for each individual letter.
 * 
 * Parameters:
 *   c - The character to write (ASCII value)
 */
void console_putc(char c) {
    // Add character to our software buffer (fast - just a RAM write)
    // Post-increment: use current value of buffer_pos as index, then increment it
    // Example: if buffer_pos is 3, this stores at buffer[3] and makes buffer_pos become 4
    buffer[buffer_pos++] = c;
    
    // Check if buffer is now full
    // If so, we must flush to hardware before adding more characters
    // Otherwise we'd overflow the buffer and corrupt memory!
    if (buffer_pos >= BUFFER_SIZE) {
        flush_buffer();  // Send all buffered characters to hardware
        // After flush, buffer_pos is reset to 0 and we can continue buffering
    }
}

/*
 * console_puts() - Write a null-terminated string to console
 * 
 * This is a convenience function for writing complete strings.
 * It handles newline conversion and ensures the string is fully sent.
 * 
 * Newline Handling:
 * Unix/Linux uses '\n' (Line Feed) for newlines
 * Old terminals and Windows use '\r\n' (Carriage Return + Line Feed)
 * We convert '\n' to '\r\n' for maximum terminal compatibility
 * This ensures the cursor returns to column 0 (carriage return) before
 * moving to the next line (line feed)
 * 
 * Flushing Strategy:
 * We flush after each complete string to ensure it appears immediately
 * on the terminal. Without this, short strings might sit in the buffer
 * and not appear until the buffer fills up with other output.
 * 
 * Parameters:
 *   str - Pointer to null-terminated string to write
 */
void console_puts(const char *str) {
    // Loop through each character in the string until we hit the null terminator '\0'
    while (*str) {
        // Handle newline conversion for terminal compatibility
        // If we see a '\n' (line feed), insert a '\r' (carriage return) first
        // This moves the cursor back to column 0 before going to the next line
        if (*str == '\n') {
            console_putc('\r');  // Carriage return: move cursor to start of line
        }
        
        // Write the actual character (whether it's '\n' or any other character)
        console_putc(*str);
        
        // Move to next character in the string
        str++;
    }
    
    // Flush the buffer to ensure the string appears immediately on the terminal
    // Without this, characters would sit in the buffer until:
    // 1. The buffer fills up (64 characters), OR
    // 2. Another flush happens
    // By flushing after each string, we get immediate feedback on the screen
    flush_buffer();
}

/*
 * console_getc() - Read a single character from console (non-blocking)
 *
 * Uses ESP-IDF's standard getchar() which is already configured to read
 * from the USB Serial/JTAG peripheral. ESP-IDF's VFS (Virtual File System)
 * driver handles all the low-level register access for us.
 *
 * This approach is simpler and more reliable than direct register access,
 * since ESP-IDF's bootloader has already initialized the USB Serial/JTAG
 * peripheral and installed the VFS driver.
 *
 * This is non-blocking: it returns immediately whether or not data is available.
 * This allows the main loop to poll for input without getting stuck waiting.
 *
 * Returns:
 *   -1 if no character is available (EOF)
 *   0-255 (as int) if a character was read
 */
int console_getc(void) {
    // Use ESP-IDF's standard getchar() function
    // ESP-IDF has already configured stdin to be non-blocking
    // and connected to USB Serial/JTAG
    int c = getchar();

    // getchar() returns EOF (-1) when no data is available
    // This is perfect for our non-blocking polling loop
    return c;
}
