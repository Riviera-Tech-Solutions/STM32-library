/**
 * @file stm32_gpio.c
 * @brief STM32 GPIO driver – all families. Define one of STM32F0, STM32F1, STM32F2, STM32F3,
 * STM32F4, STM32F7, STM32G0, STM32G4, STM32H5, STM32H7, STM32L0, STM32L1, STM32L4, STM32L4+,
 * STM32L5, STM32U5, STM32WB, STM32WL in your project.
 * Register-level; no HAL dependency.
 */

#include "stm32_gpio.h"

/* ----------------------------------------------------------------------------
 * STM32F0 (RM0091): AHBENR @ 0x40021014, GPIOA @ 0x48000000, step 0x400
 * MODER 0x00, ODR 0x14, IDR 0x10, PUPDR 0x0C
 * ---------------------------------------------------------------------------- */
#if defined(STM32F0)
#define RCC_BASE        0x40021000U
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14U))
#define GPIO_BASE(port) (0x48000000U + (uint32_t)(port) * 0x400U)
#define GPIO_MODER(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x14U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x10U))
#define GPIO_PUPDR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#define IOPAEN_BIT      (1U << 17)
#endif

/* ----------------------------------------------------------------------------
 * STM32F1/F2/L1 (RM0008 style): APB2ENR bit 2 = IOPAEN, GPIOA @ 0x40010800
 * CRL 0x00 (pins 0-7), CRH 0x04 (pins 8-15), ODR 0x0C, IDR 0x08. No PUPDR; pull via ODR+CR.
 * ---------------------------------------------------------------------------- */
#if defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32L1)
#define RCC_BASE        0x40021000U
#define RCC_APB2ENR      (*(volatile uint32_t *)(RCC_BASE + 0x18U))
#define IOPAEN_BIT       (1U << 2)
#define GPIO_BASE(port) (0x40010800U + (uint32_t)(port) * 0x400U)
#define GPIO_CRL(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_CRH(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x04U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x08U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#endif

/* ----------------------------------------------------------------------------
 * STM32F4 (RM0090): AHB1ENR @ 0x40023830, GPIOA @ 0x40020000, step 0x400
 * ---------------------------------------------------------------------------- */
#if defined(STM32F4)
#define RCC_BASE        0x40023800U
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define GPIO_BASE(port) (0x40020000U + (uint32_t)(port) * 0x400U)
#define GPIO_MODER(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x14U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x10U))
#define GPIO_PUPDR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#define GPIOAEN_BIT     (1U << 0)
#endif

/* ----------------------------------------------------------------------------
 * STM32F7 (RM0410): same as F4 – AHB1ENR, GPIO 0x40020000
 * ---------------------------------------------------------------------------- */
#if defined(STM32F7)
#define RCC_BASE        0x40023800U
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define GPIO_BASE(port) (0x40020000U + (uint32_t)(port) * 0x400U)
#define GPIO_MODER(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x14U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x10U))
#define GPIO_PUPDR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#define GPIOAEN_BIT     (1U << 0)
#endif

/* ----------------------------------------------------------------------------
 * STM32G0 (RM0454): IOPENR @ 0x40021034, GPIOA @ 0x50000000
 * ---------------------------------------------------------------------------- */
#if defined(STM32G0)
#define RCC_BASE        0x40021000U
#define RCC_IOPENR      (*(volatile uint32_t *)(RCC_BASE + 0x34U))
#define GPIO_BASE(port) (0x50000000U + (uint32_t)(port) * 0x400U)
#define GPIO_MODER(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x14U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x10U))
#define GPIO_PUPDR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#define IOPAEN_BIT      (1U << 0)
#endif

/* ----------------------------------------------------------------------------
 * STM32G4 / L4 / L4+ / WB / WL: AHB2ENR, GPIO @ 0x48000000
 * ---------------------------------------------------------------------------- */
#if defined(STM32G4) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32WB) || defined(STM32WL)
#define RCC_BASE        0x40021000U
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x34U))
#define GPIO_BASE(port) (0x48000000U + (uint32_t)(port) * 0x400U)
#define GPIO_MODER(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x14U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x10U))
#define GPIO_PUPDR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#define GPIOAEN_BIT     (1U << 0)
#endif

/* ----------------------------------------------------------------------------
 * STM32L0: same as G0 – IOPENR, GPIO 0x50000000
 * ---------------------------------------------------------------------------- */
#if defined(STM32L0)
#define RCC_BASE        0x40021000U
#define RCC_IOPENR      (*(volatile uint32_t *)(RCC_BASE + 0x34U))
#define GPIO_BASE(port) (0x50000000U + (uint32_t)(port) * 0x400U)
#define GPIO_MODER(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x14U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x10U))
#define GPIO_PUPDR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#define IOPAEN_BIT      (1U << 0)
#endif

/* ----------------------------------------------------------------------------
 * STM32H7 (RM0433): AHB4ENR in D3 RCC, GPIO 0x58020800
 * ---------------------------------------------------------------------------- */
#if defined(STM32H7)
#define RCC_D3_BASE     0x58024400U
#define RCC_AHB4ENR     (*(volatile uint32_t *)(RCC_D3_BASE + 0x0E0U))
#define GPIO_BASE(port) (0x58020800U + (uint32_t)(port) * 0x400U)
#define GPIO_MODER(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x14U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x10U))
#define GPIO_PUPDR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#define GPIOAEN_BIT     (1U << 0)
#endif

/* ----------------------------------------------------------------------------
 * STM32H5 / L5 / U5 (Cortex-M33): AHB2ENR, GPIO 0x42010800 (or 0x50000000)
 * ---------------------------------------------------------------------------- */
#if defined(STM32H5) || defined(STM32L5) || defined(STM32U5)
#define RCC_BASE        0x40021000U
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x34U))
#define GPIO_BASE(port) (0x42010800U + (uint32_t)(port) * 0x400U)
#define GPIO_MODER(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x00U))
#define GPIO_ODR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x14U))
#define GPIO_IDR(p)     (*(volatile uint32_t *)(GPIO_BASE(p) + 0x10U))
#define GPIO_PUPDR(p)   (*(volatile uint32_t *)(GPIO_BASE(p) + 0x0CU))
#define GPIOAEN_BIT     (1U << 0)
#endif

static void gpio_clock_enable_all_families(uint8_t port) {
#if defined(STM32F0)
  if (port <= 5u) RCC_AHBENR |= (1U << (17u + port));
#elif defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32L1)
  if (port <= 5u) RCC_APB2ENR |= (IOPAEN_BIT << port);
#elif defined(STM32F4) || defined(STM32F7)
  if (port <= 6u) RCC_AHB1ENR |= (GPIOAEN_BIT << port);
#elif defined(STM32G0) || defined(STM32L0)
  if (port <= 10u) RCC_IOPENR |= (IOPAEN_BIT << port);
#elif defined(STM32G4) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32WB) || defined(STM32WL)
  if (port <= 6u) RCC_AHB2ENR |= (GPIOAEN_BIT << port);
#elif defined(STM32H7)
  if (port <= 6u) RCC_AHB4ENR |= (GPIOAEN_BIT << port);
#elif defined(STM32H5) || defined(STM32L5) || defined(STM32U5)
  if (port <= 6u) RCC_AHB2ENR |= (GPIOAEN_BIT << port);
#endif
}

void gpio_set_mode(uint8_t port, uint8_t pin, gpio_mode_t mode) {
  if (pin > 15u) return;
#if defined(STM32F0) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L0) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32WB) || defined(STM32WL) || defined(STM32H7) || defined(STM32H5) || defined(STM32L5) || defined(STM32U5)
  gpio_clock_enable_all_families(port);
  volatile uint32_t *moder = &GPIO_MODER(port);
  uint32_t shift = (uint32_t)pin * 2u;
  uint32_t mask = 3u << shift;
  *moder = (*moder & ~mask) | ((uint32_t)mode << shift);
#elif defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32L1)
  gpio_clock_enable_all_families(port);
  volatile uint32_t *cr = (pin < 8u) ? &GPIO_CRL(port) : &GPIO_CRH(port);
  uint8_t p = (pin < 8u) ? pin : (pin - 8u);
  uint32_t shift = (uint32_t)p * 4u;
  uint32_t mask = 0xFu << shift;
  uint32_t val;
  if (mode == GPIO_MODE_OUTPUT) val = 0x1u;       /* 01 00 = output push-pull 10MHz */
  else if (mode == GPIO_MODE_INPUT) val = 0x4u;    /* 00 01 = input floating (pull set separately) */
  else if (mode == GPIO_MODE_AF) val = 0x2u;
  else val = 0x0u; /* analog */
  *cr = (*cr & ~mask) | (val << shift);
#else
  (void)port;
  (void)pin;
  (void)mode;
#endif
}

void gpio_set_pull(uint8_t port, uint8_t pin, gpio_pull_t pull) {
  if (pin > 15u) return;
#if defined(STM32F0) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L0) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32WB) || defined(STM32WL) || defined(STM32H7) || defined(STM32H5) || defined(STM32L5) || defined(STM32U5)
  gpio_clock_enable_all_families(port);
  volatile uint32_t *pupdr = &GPIO_PUPDR(port);
  uint32_t shift = (uint32_t)pin * 2u;
  uint32_t mask = 3u << shift;
  *pupdr = (*pupdr & ~mask) | ((uint32_t)pull << shift);
#elif defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32L1)
  gpio_clock_enable_all_families(port);
  /* F1: input pull = CR 10 (0x8), ODR 1 = pull-up, 0 = pull-down */
  volatile uint32_t *cr = (pin < 8u) ? &GPIO_CRL(port) : &GPIO_CRH(port);
  uint8_t p = (pin < 8u) ? pin : (pin - 8u);
  uint32_t shift = (uint32_t)p * 4u;
  uint32_t mask = 0xFu << shift;
  uint32_t cr_val = (*cr >> shift) & 0xFu;
  if ((cr_val & 3u) == 0u) { /* input mode */
    *cr = (*cr & ~mask) | (0x8u << shift); /* 10 = input with pull */
    uint32_t odr = GPIO_ODR(port);
    if (pull == GPIO_PULL_UP) GPIO_ODR(port) = odr | (1u << pin);
    else GPIO_ODR(port) = odr & ~(1u << pin);
  }
#else
  (void)port;
  (void)pin;
  (void)pull;
#endif
}

void gpio_write(uint8_t port, uint8_t pin, bool high) {
  if (pin > 15u) return;
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L0) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32WB) || defined(STM32WL) || defined(STM32H7) || defined(STM32H5) || defined(STM32L5) || defined(STM32U5)
  uint32_t bit = 1u << pin;
  if (high) GPIO_ODR(port) |= bit;
  else GPIO_ODR(port) &= ~bit;
#else
  (void)port;
  (void)pin;
  (void)high;
#endif
}

bool gpio_read(uint8_t port, uint8_t pin) {
  if (pin > 15u) return false;
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L0) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32WB) || defined(STM32WL) || defined(STM32H7) || defined(STM32H5) || defined(STM32L5) || defined(STM32U5)
  return (GPIO_IDR(port) & (1u << pin)) != 0u;
#else
  (void)port;
  (void)pin;
  return false;
#endif
}
