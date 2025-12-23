#include "i2c.h"
#include <stdint.h>
#include <stdbool.h>

// GPIO register base addresses
#define GPIO_BASE           0x60004000
#define IO_MUX_BASE         0x60009000

// GPIO registers
#define GPIO_ENABLE_REG     (GPIO_BASE + 0x0020)
#define GPIO_OUT_REG        (GPIO_BASE + 0x0004)
#define GPIO_OUT_W1TS_REG   (GPIO_BASE + 0x0008)
#define GPIO_OUT_W1TC_REG   (GPIO_BASE + 0x000C)
#define GPIO_IN_REG         (GPIO_BASE + 0x003C)

// IO MUX registers
#define GPIO_PIN_MUX_REG(n) (IO_MUX_BASE + 0x0004 + (n * 4))

// IO MUX configuration bits
#define FUN_IE              (1 << 9)
#define FUN_DRV_SHIFT       10
#define MCU_SEL_SHIFT       12
#define FUN_WPU             (1 << 7)   // Weak pull-up
#define FUN_WPD             (1 << 8)   // Weak pull-down

// Register access macros
#define REG_WRITE(addr, val) (*((volatile uint32_t *)(addr)) = (val))
#define REG_READ(addr)       (*((volatile uint32_t *)(addr)))
#define REG_SET_BIT(addr, bit)   REG_WRITE(addr, REG_READ(addr) | (bit))
#define REG_CLR_BIT(addr, bit)   REG_WRITE(addr, REG_READ(addr) & ~(bit))

// I2C timing delays (in CPU cycles for bit-banging)
static uint32_t i2c_delay_cycles;
static int scl_gpio;
static int sda_gpio;

// Delay function for I2C timing
static void i2c_delay(void) {
    for (volatile uint32_t i = 0; i < i2c_delay_cycles; i++);
}

// Set pin as open-drain output with pull-up
static void gpio_set_opendrain(int gpio_num) {
    if (gpio_num < 0 || gpio_num > 21) return;

    uint32_t mux_reg = GPIO_PIN_MUX_REG(gpio_num);
    uint32_t mux_val = REG_READ(mux_reg);

    // Set function to GPIO
    mux_val &= ~(0x7 << MCU_SEL_SHIFT);
    mux_val |= (1 << MCU_SEL_SHIFT);

    // Enable input and pull-up
    mux_val |= FUN_IE | FUN_WPU;
    mux_val &= ~FUN_WPD;

    // Set drive strength
    mux_val &= ~(0x3 << FUN_DRV_SHIFT);
    mux_val |= (2 << FUN_DRV_SHIFT);

    REG_WRITE(mux_reg, mux_val);

    // Enable as output
    REG_SET_BIT(GPIO_ENABLE_REG, (1 << gpio_num));
}

// Set SCL high (release line, pulled up by resistor)
static void scl_high(void) {
    REG_WRITE(GPIO_OUT_W1TS_REG, (1 << scl_gpio));
    i2c_delay();
}

// Set SCL low
static void scl_low(void) {
    REG_WRITE(GPIO_OUT_W1TC_REG, (1 << scl_gpio));
    i2c_delay();
}

// Set SDA high (release line)
static void sda_high(void) {
    REG_WRITE(GPIO_OUT_W1TS_REG, (1 << sda_gpio));
    i2c_delay();
}

// Set SDA low
static void sda_low(void) {
    REG_WRITE(GPIO_OUT_W1TC_REG, (1 << sda_gpio));
    i2c_delay();
}

// Read SDA state
static bool sda_read(void) {
    return (REG_READ(GPIO_IN_REG) & (1 << sda_gpio)) != 0;
}

void i2c_init(const i2c_config_t *config) {
    scl_gpio = config->scl_pin;
    sda_gpio = config->sda_pin;

    // Calculate delay cycles for desired frequency
    // ESP32-C3 runs at 160MHz, adjust for desired I2C frequency
    // For 100kHz I2C: period = 10us, half-period = 5us
    // At 160MHz: 5us = 800 cycles
    i2c_delay_cycles = (160000000 / config->freq_hz) / 4;

    // Configure pins as open-drain
    gpio_set_opendrain(scl_gpio);
    gpio_set_opendrain(sda_gpio);

    // Initialize both lines high
    sda_high();
    scl_high();
}

bool i2c_start(void) {
    // SDA goes low while SCL is high
    sda_high();
    scl_high();
    sda_low();
    scl_low();
    return true;
}

void i2c_stop(void) {
    // SDA goes high while SCL is high
    sda_low();
    scl_high();
    sda_high();
}

bool i2c_write_byte(uint8_t data) {
    // Write 8 bits
    for (int i = 7; i >= 0; i--) {
        if (data & (1 << i)) {
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
    bool ack = !sda_read();  // ACK is active low
    scl_low();

    return ack;
}

uint8_t i2c_read_byte(bool ack) {
    uint8_t data = 0;

    sda_high();  // Release SDA for reading

    // Read 8 bits
    for (int i = 7; i >= 0; i--) {
        scl_high();
        if (sda_read()) {
            data |= (1 << i);
        }
        scl_low();
    }

    // Send ACK/NACK
    if (ack) {
        sda_low();
    } else {
        sda_high();
    }
    scl_high();
    scl_low();
    sda_high();  // Release SDA

    return data;
}

bool i2c_write(uint8_t addr, const uint8_t *data, uint32_t len) {
    if (!i2c_start()) {
        return false;
    }

    // Write device address with write bit
    if (!i2c_write_byte(addr << 1)) {
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

bool i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, uint32_t len) {
    if (!i2c_start()) {
        return false;
    }

    // Write device address with write bit
    if (!i2c_write_byte(addr << 1)) {
        i2c_stop();
        return false;
    }

    // Write register address
    if (!i2c_write_byte(reg)) {
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
