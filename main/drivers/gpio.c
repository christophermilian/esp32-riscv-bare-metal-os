#include <stdint.h>
#include "gpio.h"

// GPIO register base addresses
#define GPIO_BASE           0x60004000
#define IO_MUX_BASE         0x60009000

// GPIO registers
#define GPIO_ENABLE_REG     (GPIO_BASE + 0x0020)
#define GPIO_OUT_REG        (GPIO_BASE + 0x0004)
#define GPIO_OUT_W1TS_REG   (GPIO_BASE + 0x0008)  // Write 1 to set
#define GPIO_OUT_W1TC_REG   (GPIO_BASE + 0x000C)  // Write 1 to clear

// IO MUX registers (one per GPIO)
#define GPIO_PIN_MUX_REG(n) (IO_MUX_BASE + 0x0004 + (n * 4))

// IO MUX configuration bits
#define FUN_IE              (1 << 9)   // Input enable
#define FUN_DRV_SHIFT       10         // Drive strength
#define MCU_SEL_SHIFT       12         // Function select

// Register access macros
#define REG_WRITE(addr, val) (*((volatile uint32_t *)(addr)) = (val))
#define REG_READ(addr)       (*((volatile uint32_t *)(addr)))
#define REG_SET_BIT(addr, bit)   REG_WRITE(addr, REG_READ(addr) | (bit))
#define REG_CLR_BIT(addr, bit)   REG_WRITE(addr, REG_READ(addr) & ~(bit))

void gpio_set_output(int gpio_num) {
    if (gpio_num < 0 || gpio_num > 21) return;  // ESP32-C3 has GPIO 0-21

    // Configure IO MUX for GPIO function
    uint32_t mux_reg = GPIO_PIN_MUX_REG(gpio_num);
    uint32_t mux_val = REG_READ(mux_reg);

    // Set function to GPIO (function 1)
    mux_val &= ~(0x7 << MCU_SEL_SHIFT);  // Clear function bits
    mux_val |= (1 << MCU_SEL_SHIFT);     // Set to GPIO function

    // Set drive strength to medium (2)
    mux_val &= ~(0x3 << FUN_DRV_SHIFT);
    mux_val |= (2 << FUN_DRV_SHIFT);

    REG_WRITE(mux_reg, mux_val);

    // Enable output
    REG_SET_BIT(GPIO_ENABLE_REG, (1 << gpio_num));
}

void gpio_set_high(int gpio_num) {
    if (gpio_num < 0 || gpio_num > 21) return;
    REG_WRITE(GPIO_OUT_W1TS_REG, (1 << gpio_num));
}

void gpio_set_low(int gpio_num) {
    if (gpio_num < 0 || gpio_num > 21) return;
    REG_WRITE(GPIO_OUT_W1TC_REG, (1 << gpio_num));
}

void gpio_toggle(int gpio_num) {
    if (gpio_num < 0 || gpio_num > 21) return;
    uint32_t current = REG_READ(GPIO_OUT_REG);
    if (current & (1 << gpio_num)) {
        gpio_set_low(gpio_num);
    } else {
        gpio_set_high(gpio_num);
    }
}
