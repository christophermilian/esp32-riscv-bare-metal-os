#ifndef GPIO_H
#define GPIO_H

// Initialize a GPIO pin as output
void gpio_set_output(int gpio_num);

// Set GPIO pin high
void gpio_set_high(int gpio_num);

// Set GPIO pin low
void gpio_set_low(int gpio_num);

// Toggle GPIO pin
void gpio_toggle(int gpio_num);

#endif // GPIO_H
