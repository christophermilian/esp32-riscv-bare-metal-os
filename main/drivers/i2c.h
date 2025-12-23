#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>

// I2C configuration
typedef struct {
    int scl_pin;
    int sda_pin;
    uint32_t freq_hz;
} i2c_config_t;

// Initialize I2C master
void i2c_init(const i2c_config_t *config);

// Start condition
bool i2c_start(void);

// Stop condition
void i2c_stop(void);

// Write byte (returns true if ACK received)
bool i2c_write_byte(uint8_t data);

// Read byte
uint8_t i2c_read_byte(bool ack);

// Write multiple bytes to device
bool i2c_write(uint8_t addr, const uint8_t *data, uint32_t len);

// Write register data
bool i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, uint32_t len);

#endif // I2C_H
