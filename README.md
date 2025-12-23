# ESP32-C3 RISC-V Bare-Metal OS

A minimal bare-metal operating system for the ESP32-C3 (RISC-V) microcontroller with OLED display support.

## Features

- Bare-metal RISC-V code execution
- UART console output
- GPIO control
- I2C communication (bit-banged)
- SSD1306 OLED display driver (128x64)
- Interactive command shell (serial input → OLED output)
- Custom bootloader (work in progress)
- **Rust implementation** available as alternative to C version

## Hardware Support

### Supported Board
- **Seeed XIAO ESP32-C3** (or any ESP32-C3 board)

### Peripherals
- **SSD1306 OLED Display** (128x64, I2C)
  - I2C Address: 0x3C (default) or 0x3D
  - Pins: GPIO6 (SDA), GPIO7 (SCL)

## Project Structure

```
esp32-riscv-bare-metal-os/
├── main/
│   ├── main.c                    # Your application code
│   ├── shell.c/h                # Interactive command shell
│   ├── CMakeLists.txt           # Build configuration
│   │
│   ├── startup/                  # Low-level startup code
│   │   ├── boot.S               # RISC-V entry point
│   │   └── linker.ld            # Memory layout
│   │
│   ├── drivers/                  # Hardware drivers (HAL)
│   │   ├── gpio.c/h             # GPIO control
│   │   ├── i2c.c/h              # I²C peripheral
│   │   └── console.c/h          # USB Serial/JTAG console
│   │
│   ├── devices/                  # External device drivers
│   │   └── ssd1306.c/h          # OLED display driver
│   │
│   └── assets/                   # Fonts, images, etc.
│       └── font5x7.h            # 5x7 character font
│
├── bootloader/                   # Custom bootloader (WIP)
├── rust/                         # Rust implementation (alternative to C)
│   ├── src/                     # Rust source files
│   │   ├── main.rs              # Entry point
│   │   ├── console.rs           # USB Serial/JTAG driver
│   │   ├── gpio.rs              # GPIO driver
│   │   ├── i2c.rs               # I2C driver
│   │   ├── ssd1306.rs           # OLED driver
│   │   ├── shell.rs             # Command shell
│   │   └── font5x7.rs           # Font data
│   ├── Cargo.toml               # Rust project config
│   └── README.md                # Rust-specific instructions
│
├── build/                        # Build artifacts (generated)
└── README.md                    # This file
```

## Getting Started

### Prerequisites
- ESP-IDF v5.0 or later
- XIAO ESP32-C3 board or compatible
- SSD1306 OLED display (optional)

### Wiring (for SSD1306 OLED)

| OLED Pin | ESP32-C3 Pin | Label |
|----------|-------------|-------|
| VCC | 3V3 or 5V | Power |
| GND | GND | Ground |
| SCL | GPIO7 | D5 |
| SDA | GPIO6 | D4 |

### Building and Flashing

#### C Version (ESP-IDF)

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Build
idf.py build

# Flash and monitor
idf.py flash monitor
```

#### Rust Version (Alternative)

```bash
# Navigate to rust directory
cd rust

# Build and flash
cargo run --release
```

See [rust/README.md](rust/README.md) for detailed Rust setup instructions.

## Code Examples

### Using the OLED Display

```c
#include "ssd1306.h"

// Initialize display
ssd1306_config_t config = {
    .i2c_addr = SSD1306_I2C_ADDR_DEFAULT,
    .scl_pin = 7,
    .sda_pin = 6
};
ssd1306_init(&config);

// Draw text
ssd1306_clear();
ssd1306_draw_string(0, 0, "Hello World!");
ssd1306_display();
```

### Available Display Functions

- `ssd1306_init()` - Initialize display
- `ssd1306_clear()` - Clear buffer
- `ssd1306_display()` - Update screen
- `ssd1306_set_pixel()` - Set individual pixel
- `ssd1306_draw_char()` - Draw character
- `ssd1306_draw_string()` - Draw text string
- `ssd1306_fill_rect()` - Draw filled rectangle
- `ssd1306_set_contrast()` - Adjust brightness
- `ssd1306_display_on()` - Turn on/off
- `ssd1306_invert_display()` - Invert colors

### Using the Interactive Shell

The shell accepts input from the USB Serial console and displays output on the OLED:

```c
#include "shell.h"

// Initialize the shell
shell_init();

// In your main loop, process incoming serial characters
while (1) {
    char c = console_getchar();  // Read from USB Serial
    if (c != 0) {
        shell_process_char(c);    // Process and display on OLED
    }
}
```

**Shell Features:**
- Command history
- Line editing (backspace support)
- Echo commands to both serial console and OLED display
- Extensible command system

---

# ESP32-C3 Memory Mapping Explained

## Overview

The ESP32-C3 uses a **modified Harvard architecture** where the same physical memory (SRAM) can be accessed through different address ranges. This document explains why and how this works.

---

## The Memory Map (from Datasheet Section 3.1.4)

### Key Memory Regions

| Address Range | Size | Purpose | Bus Type |
|--------------|------|---------|----------|
| **0x3FC80000 - 0x3FCDF000** | 400KB | SRAM (Data Access) | Data Bus |
| **0x4037C000 - 0x403DFFFF** | 400KB | SRAM (Instruction Access) | Instruction Bus |
| **0x42000000 - 0x427FFFFF** | 4MB | Flash Memory | Flash Bus |
| **0x60000000 - 0x600D0FFF** | ~832KB | Peripherals | Peripheral Bus |

### Important Note About Gray Blocks

**From the datasheet:** "The memory space with gray background is not available for use."

This means addresses like:
- 0x3FCE0000 and above are **NOT accessible** even though they're in the address space
- Always check the datasheet to verify accessible ranges!

---

## Why Two Addresses for the Same SRAM?

The ESP32-C3 diagram shows **two arrows pointing to the SRAM block**:

```
Address Range 1: 0x3FC80000 - 0x3FCDF000  ──┐
                                             ├─→ [SAME PHYSICAL SRAM]
Address Range 2: 0x4037C000 - 0x403DFFFF  ──┘
```

### This is Called "Memory Aliasing"

The same physical memory byte can be accessed through different addresses:

```
Physical SRAM Byte #0:
├─ Data Access:        0x3FC80000
└─ Instruction Access: 0x4037C000
    ↓
   SAME physical byte!
```

---

## Harvard Architecture vs Von Neumann Architecture

### Von Neumann Architecture (Traditional)
```
     CPU
      |
   Single Bus
      |
   ┌──┴──┐
   │     │
  Code  Data
```
- One bus for everything
- CPU can't fetch instructions and read/write data simultaneously
- Simpler, but slower

### Harvard Architecture (ESP32-C3)
```
        CPU
       /   \
      /     \
  I-Bus     D-Bus
    |         |
  Code      Data
```
- Separate buses for instructions and data
- CPU can fetch next instruction WHILE reading/writing data
- More complex, but faster (parallelism)

---

## Practical Implications for Your Code

### 1. **Stack and Variables (Use Data Bus)**

**For boot.S:**
```assembly
# Stack pointer should use DATA BUS address
lui sp, 0x3FCDF      # 0x3FCDF000 (data bus)
```

**For linker.ld:**
```ld
MEMORY {
    dram : ORIGIN = 0x3FC80000, LENGTH = 0x5F000
}

SECTIONS {
    .data : { *(.data*) } > dram
    .bss  : { *(.bss*) } > dram
}
```

### 2. **When to Use Instruction Bus (Advanced)**

You'd use the instruction bus address (0x4037C000) when:

1. **Executing code from RAM** (for speed)
   ```c
   // Copy frequently-used function to RAM
   void fast_function(void) __attribute__((section(".iram")));
   ```

2. **Self-modifying code** (rare, advanced)

3. **Tight loops** that need maximum performance

**For now:** Just use the data bus addresses (0x3FC80000). Instruction bus is an optimization for later.

---

## Memory Map Visual Summary

```
Address Space (32-bit = 4GB total)

0x00000000
    ↓
[Reserved/ROM]
    ↓
0x3FC80000  ┌─────────────────┐
            │                 │
            │  SRAM (Data)    │ ← Use this for stack, .data, .bss
            │  400 KB         │
0x3FCDF000  ├─────────────────┤
            │                 │
            │  [Inaccessible] │ ← Gray blocks in datasheet
            │                 │
0x3FEF0000  └─────────────────┘

0x4037C000  ┌─────────────────┐
            │                 │
            │  SRAM (Code)    │ ← Same physical SRAM!
            │  400 KB         │
            │                 │
0x403DF000  └─────────────────┘

0x42000000  ┌─────────────────┐
            │                 │
            │  Flash Memory   │ ← Your code lives here
            │  4 MB           │
            │                 │
0x427FFFFF  └─────────────────┘

0x60000000  ┌─────────────────┐
            │                 │
            │  Peripherals    │ ← UART, GPIO, I2C, etc.
            │  (Memory-mapped)│
            │                 │
0x600D0FFF  └─────────────────┘
```

---

## Common Mistakes to Avoid

### ❌ Wrong: Using Inaccessible Memory
```assembly
lui sp, 0x3FCE0      # CRASH! This is gray/inaccessible
```

### ✅ Correct: Using Valid SRAM Range
```assembly
lui sp, 0x3FCDF      # Good! Within accessible range
```

### ❌ Wrong: Mixing Bus Types
```c
// Don't put instruction pointer in data bus address
void (*func_ptr)(void) = 0x3FC80000;  // Wrong!
```

### ✅ Correct: Right Bus for Right Purpose
```c
// Stack in data bus
uint32_t stack_var = 0x3FCDF000;  // Correct!
```

---

## Key Takeaways

1. **Two addresses, same physical memory** - Harvard architecture feature
2. **Use 0x3FC80000 range** for stack, variables (data bus)
3. **Use 0x4037C000 range** only for executing code from RAM (advanced)
4. **Gray blocks in datasheet = inaccessible** - always verify addresses!
5. **SRAM is 400KB** from 0x3FC80000 to ~0x3FCDF000

---

## When You Come Back to This

**Quick Reference:**
- Stack pointer: `0x3FCDF000` (top of SRAM, grows down)
- SRAM start: `0x3FC80000`
- SRAM size: `400KB (0x5F000 bytes)`
- Flash start: `0x42000000`
- Peripherals: `0x60000000`

**Remember:** The ESP32-C3 is Harvard architecture - separate buses for code and data access to the same physical memory. For basic bare-metal work, just use the data bus addresses (0x3FC80000 range).

---

## Further Reading

- ESP32-C3 Technical Reference Manual - Chapter 3: Memory System
- RISC-V Privileged Specification - Memory Models
- Understanding Harvard vs Von Neumann Architecture

Good luck with your bare-metal OS! 🚀

## Mental Tricks
- "LUI adds 000 to the end" (three hex zeros = 12 bits)

---

## Custom Bootloader

### Overview

This project includes a custom bare-metal bootloader in the `bootloader/` directory. While not yet fully integrated with ESP-IDF's build system, it demonstrates the principles of a minimal bootloader.

### Bootloader Files

- **[bootloader/bootloader_start.S](bootloader/bootloader_start.S)** - Assembly entry point
- **[bootloader/bootloader_main.c](bootloader/bootloader_main.c)** - Bootloader logic
- **[bootloader/bootloader.ld](bootloader/bootloader.ld)** - Bootloader linker script

### Boot Sequence

1. **ROM Bootloader** (chip ROM)
   - Validates and loads 2nd stage bootloader from flash offset 0x1000
   - Jumps to bootloader entry point

2. **Custom Bootloader** (our code at 0x1000)
   - Initializes stack
   - Prints boot message via UART
   - Validates application
   - Jumps to application at 0x42000020

3. **Application** (our main code at 0x10000)
   - Starts at `_start` in [main/boot.S](main/boot.S)
   - Initializes C runtime (copy .data, clear .bss)
   - Calls `main()`

### Current Status

The custom bootloader is **created but not yet integrated**. The project currently uses ESP-IDF's minimal bootloader for simplicity.

To fully integrate the custom bootloader, you would need to:
1. Replace ESP-IDF's bootloader component
2. Ensure proper flash layout (bootloader at 0x1000, app at 0x10000)
3. Handle image verification and loading
4. Configure CMake to build both bootloader and app separately

For learning purposes, you can examine the bootloader code to understand the boot process, then gradually replace ESP-IDF components as you need more control.
