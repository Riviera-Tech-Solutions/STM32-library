/**
 * @file stm32_gpio.h
 * @brief STM32 GPIO driver – public API
 * Define one of STM32F0, F1, F2, F3, F4, F7, G0, G4, H5, H7, L0, L1, L4, L4+, L5, U5, WB, WL in your build.
 */

#ifndef STM32_GPIO_H
#define STM32_GPIO_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  GPIO_MODE_INPUT   = 0,
  GPIO_MODE_OUTPUT  = 1,
  GPIO_MODE_AF      = 2,
  GPIO_MODE_ANALOG  = 3
} gpio_mode_t;

typedef enum {
  GPIO_PULL_NONE = 0,
  GPIO_PULL_UP   = 1,
  GPIO_PULL_DOWN = 2
} gpio_pull_t;

/* This API exposes ports A..G. Narrow this mask for your part/package. */
#ifndef GPIO_PORT_MASK
#define GPIO_PORT_MASK 0x7FU
#endif

/** Port and pin: port 0 = GPIOA, 1 = GPIOB, ... ; pin 0..15 */
void gpio_set_mode(uint8_t port, uint8_t pin, gpio_mode_t mode);
void gpio_set_pull(uint8_t port, uint8_t pin, gpio_pull_t pull);
void gpio_write(uint8_t port, uint8_t pin, bool high);
bool gpio_read(uint8_t port, uint8_t pin);

/* Configure AF0..AF15 on MODER families; F1 requires AFIO remap externally
 * and returns false. Configure AF/type/pull before switching the pin to AF. */
bool gpio_set_af(uint8_t port, uint8_t pin, uint8_t alternate);
bool gpio_set_open_drain(uint8_t port, uint8_t pin, bool open_drain);

#endif /* STM32_GPIO_H */
