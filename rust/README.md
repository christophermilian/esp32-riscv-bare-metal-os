# ESP32-C3 RISC-V Bare Metal OS - Rust Version

A bare-metal operating system for the ESP32-C3 RISC-V microcontroller, written in Rust.

## Features

- Direct hardware register access (no RTOS)
- USB Serial/JTAG console driver
- GPIO driver
- Bit-banged I2C master
- SSD1306 OLED display driver (128x64)
- Interactive shell with commands

## Prerequisites

1. Install Rust (via rustup):
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```

2. Install the ESP Rust toolchain:
   ```bash
   cargo install espup
   espup install
   ```

3. Install espflash for flashing:
   ```bash
   cargo install espflash
   ```

4. Source the ESP environment (add to your shell profile):
   ```bash
   . $HOME/export-esp.sh
   ```

## Building

```bash
cd rust
cargo build --release
```

## Flashing

Connect your ESP32-C3 board via USB and run:

```bash
cargo run --release
```

This will build, flash, and open a serial monitor.

## Project Structure

```
rust/
├── Cargo.toml           # Rust project configuration
├── .cargo/
│   └── config.toml      # Cargo build configuration
├── rust-toolchain.toml  # Rust toolchain specification
└── src/
    ├── main.rs          # Application entry point
    ├── console.rs       # USB Serial/JTAG driver
    ├── gpio.rs          # GPIO driver
    ├── i2c.rs           # Bit-banged I2C driver
    ├── ssd1306.rs       # OLED display driver
    ├── shell.rs         # Interactive shell
    └── font5x7.rs       # 5x7 pixel font
```

## Hardware Configuration

Default pin configuration for XIAO ESP32-C3:
- **SCL**: GPIO7 (I2C clock for OLED)
- **SDA**: GPIO6 (I2C data for OLED)
- **OLED Address**: 0x3C

## Shell Commands

- `help` - Display available commands
- `clear` - Clear the OLED display
- `echo <text>` - Echo text to the display

## Comparison with C Version

This Rust implementation provides the same functionality as the original C version:
- Same hardware drivers (console, GPIO, I2C, SSD1306)
- Same shell interface
- Same memory-mapped register access patterns

The C source files are preserved in the `main/` directory for reference.

## License

Same as the original C project.
