/**
 * @file stm32_uart.c
 * @brief STM32 UART (polled). IMPLEMENTED: STM32F0, STM32F1, STM32F4, STM32F7,
 *        STM32G0, STM32L4. Other families are NOT implemented — uart_init()
 *        returns false and a #warning fires at build time.
 *
 * Two register generations, verified against ST CMSIS device headers:
 *   v1 (SR@0x00 DR@0x04 BRR@0x08 CR1@0x0C): F1, F4
 *   v2 (CR1@0x00 BRR@0x0C ISR@0x1C RDR@0x24 TDR@0x28): F0, F7, G0, L4
 *
 * Baud clocks default to what THIS library's SystemInit actually leaves
 * running (not the family's marketing maximum):
 *   F4 84 MHz APB2 / 42 MHz APB1 (PLL), F0/F1 8 MHz HSI, F7 16 MHz HSI,
 *   G0 16 MHz HSI, L4 4 MHz MSI.
 * If your application reconfigures the clock tree, override UART_PCLK1_HZ
 * (USART2) and/or UART_PCLK2_HZ (USART1) in compiler options.
 *
 * Instance map: 0 = USART1, 1 = USART2. On STM32F030x4/x6 there is no USART2
 * in hardware — instance 1 still programs the USART2 address, which reads as
 * zeros on those parts; uart_init() cannot detect this, so check your part.
 */
#include "stm32_uart.h"
#include "../common/stm32_family.h"
#include <stddef.h>
#include "../common/stm32_wait.h"

#ifndef UART_TX_TIMEOUT_LOOPS
#define UART_TX_TIMEOUT_LOOPS 1000000U
#endif

#define UART_INSTANCE_COUNT 2U

/* ---- Per-family config: bases, RCC enables, SystemInit-true clocks ------ */
#if defined(STM32F4)
#define UART_MAP_V1
#define USART1_BASE  0x40011000U
#define USART2_BASE  0x40004400U
#define RCC_APB2ENR  (*(volatile uint32_t *)(0x40023844U))
#define RCC_APB1ENR  (*(volatile uint32_t *)(0x40023840U))
#define UART_EN_USART1() (RCC_APB2ENR |= (1U << 4))
#define UART_EN_USART2() (RCC_APB1ENR |= (1U << 17))
#ifndef UART_PCLK2_HZ
#define UART_PCLK2_HZ 84000000U   /* APB2 after SystemInit's 84 MHz PLL */
#endif
#ifndef UART_PCLK1_HZ
#define UART_PCLK1_HZ 42000000U   /* APB1 = HCLK/2 */
#endif

#elif defined(STM32F7)
#define UART_MAP_V2
#define USART1_BASE  0x40011000U
#define USART2_BASE  0x40004400U
#define RCC_APB2ENR  (*(volatile uint32_t *)(0x40023844U))
#define RCC_APB1ENR  (*(volatile uint32_t *)(0x40023840U))
#define UART_EN_USART1() (RCC_APB2ENR |= (1U << 4))
#define UART_EN_USART2() (RCC_APB1ENR |= (1U << 17))
#ifndef UART_PCLK2_HZ
#define UART_PCLK2_HZ 16000000U   /* HSI; SystemInit does not start the PLL */
#endif
#ifndef UART_PCLK1_HZ
#define UART_PCLK1_HZ 16000000U
#endif

#elif defined(STM32F1)
#define UART_MAP_V1
#define USART1_BASE  0x40013800U
#define USART2_BASE  0x40004400U
#define RCC_APB2ENR  (*(volatile uint32_t *)(0x40021018U))
#define RCC_APB1ENR  (*(volatile uint32_t *)(0x4002101CU))
#define UART_EN_USART1() (RCC_APB2ENR |= (1U << 14))
#define UART_EN_USART2() (RCC_APB1ENR |= (1U << 17))
#ifndef UART_PCLK2_HZ
#define UART_PCLK2_HZ 8000000U    /* HSI */
#endif
#ifndef UART_PCLK1_HZ
#define UART_PCLK1_HZ 8000000U
#endif

#elif defined(STM32F0)
#define UART_MAP_V2
#define USART1_BASE  0x40013800U
#define USART2_BASE  0x40004400U
#define RCC_APB2ENR  (*(volatile uint32_t *)(0x40021018U))
#define RCC_APB1ENR  (*(volatile uint32_t *)(0x4002101CU))
#define UART_EN_USART1() (RCC_APB2ENR |= (1U << 14))
#define UART_EN_USART2() (RCC_APB1ENR |= (1U << 17))
#ifndef UART_PCLK2_HZ
#define UART_PCLK2_HZ 8000000U    /* HSI */
#endif
#ifndef UART_PCLK1_HZ
#define UART_PCLK1_HZ 8000000U
#endif

#elif defined(STM32G0)
#define UART_MAP_V2
#define USART1_BASE  0x40013800U
#define USART2_BASE  0x40004400U
/* RM0444/CMSIS: APBENR1 @ RCC+0x3C, APBENR2 @ RCC+0x40. (A previous revision
   wrote 0x40021058/5C — reserved space — so the clock never enabled.) */
#define RCC_APBENR1  (*(volatile uint32_t *)(0x4002103CU))
#define RCC_APBENR2  (*(volatile uint32_t *)(0x40021040U))
#define UART_EN_USART1() (RCC_APBENR2 |= (1U << 14))
#define UART_EN_USART2() (RCC_APBENR1 |= (1U << 17))
#ifndef UART_PCLK2_HZ
#define UART_PCLK2_HZ 16000000U   /* HSI16; SystemInit does not start the PLL */
#endif
#ifndef UART_PCLK1_HZ
#define UART_PCLK1_HZ 16000000U
#endif

#elif defined(STM32L4) || defined(STM32L4PLUS)
#define UART_MAP_V2
#define USART1_BASE  0x40013800U
#define USART2_BASE  0x40004400U
#define RCC_APB2ENR   (*(volatile uint32_t *)(0x40021060U))
#define RCC_APB1ENR1  (*(volatile uint32_t *)(0x40021058U))
#define UART_EN_USART1() (RCC_APB2ENR |= (1U << 14))
#define UART_EN_USART2() (RCC_APB1ENR1 |= (1U << 17))
#ifndef UART_PCLK2_HZ
#define UART_PCLK2_HZ 4000000U    /* MSI default; SystemInit does not start a PLL */
#endif
#ifndef UART_PCLK1_HZ
#define UART_PCLK1_HZ 4000000U
#endif
#endif

/* ---- Register maps ------------------------------------------------------ */
#if defined(UART_MAP_V1)
#define UART_SR(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define UART_DR(b)   (*(volatile uint32_t *)((b) + 0x04U))
#define UART_BRR(b)  (*(volatile uint32_t *)((b) + 0x08U))
#define UART_CR1(b)  (*(volatile uint32_t *)((b) + 0x0CU))
#define UART_STATUS_OFFSET 0x00U
#define CR1_UE       (1U << 13)
#define CR1_TE       (1U << 3)
#define CR1_RE       (1U << 2)
#define ST_TXE       (1U << 7)
#define ST_RXNE      (1U << 5)
#elif defined(UART_MAP_V2)
#define UART_CR1(b)  (*(volatile uint32_t *)((b) + 0x00U))
#define UART_BRR(b)  (*(volatile uint32_t *)((b) + 0x0CU))
#define UART_ISR(b)  (*(volatile uint32_t *)((b) + 0x1CU))
#define UART_RDR(b)  (*(volatile uint32_t *)((b) + 0x24U))
#define UART_TDR(b)  (*(volatile uint32_t *)((b) + 0x28U))
#define UART_STATUS_OFFSET 0x1CU
#define CR1_UE       (1U << 0)
#define CR1_TE       (1U << 3)
#define CR1_RE       (1U << 2)
#define ST_TXE       (1U << 7)
#define ST_RXNE      (1U << 5)
#endif

static uint32_t get_base(uint8_t instance) {
#if defined(UART_MAP_V1) || defined(UART_MAP_V2)
  return (instance == 0) ? USART1_BASE : USART2_BASE;
#else
  (void)instance;
  return 0;
#endif
}

static uint32_t get_pclk(uint8_t instance) __attribute__((unused));
static uint32_t get_pclk(uint8_t instance) {
#if defined(UART_MAP_V1) || defined(UART_MAP_V2)
  return (instance == 0) ? UART_PCLK2_HZ : UART_PCLK1_HZ;
#else
  (void)instance;
  return 0;
#endif
}

static bool s_uart_initialized[UART_INSTANCE_COUNT];
static uart_stats_t s_uart_stats[UART_INSTANCE_COUNT];

static bool uart_valid_instance(uint8_t instance) {
  return instance < UART_INSTANCE_COUNT && (UART_INSTANCE_MASK & (1U << instance));
}

static void uart_set_error(uint8_t instance, uart_error_t err) {
  if (!uart_valid_instance(instance)) return;
  s_uart_stats[instance].last_error = err;
}

bool uart_init(uint8_t instance, uint32_t baud) {
  if (!uart_valid_instance(instance)) return false;
  if (baud == 0U) {
    s_uart_stats[instance].init_fail++;
    uart_set_error(instance, UART_ERR_INVALID_PARAM);
    return false;
  }
#if defined(UART_MAP_V1) || defined(UART_MAP_V2)
  uint32_t pclk = get_pclk(instance);
  uint64_t divider = ((uint64_t)pclk + baud / 2U) / baud;
  uint64_t requested = divider * baud;
  uint64_t error = requested > pclk ? requested - pclk : pclk - requested;
  if (divider < 16U || divider > 65535U ||
      error * 1000000ULL > requested * UART_MAX_BAUD_ERROR_PPM) {
    s_uart_stats[instance].init_fail++;
    uart_set_error(instance, UART_ERR_INVALID_PARAM);
    return false;
  }
  if (instance == 0) UART_EN_USART1();
  else UART_EN_USART2();
  uint32_t base = get_base(instance);
  if (base == 0U) {
    s_uart_stats[instance].init_fail++;
    uart_set_error(instance, UART_ERR_BASE_UNAVAILABLE);
    return false;
  }
  UART_CR1(base) &= ~CR1_UE;
  UART_BRR(base) = (uint32_t)divider;
  UART_CR1(base) = CR1_TE | CR1_RE | CR1_UE; /* 8-N-1, oversampling by 16 */
  s_uart_initialized[instance] = true;
  s_uart_stats[instance].init_ok++;
  uart_set_error(instance, UART_ERR_NONE);
  return true;
#else
#warning "stm32_uart: implemented only for STM32F0/F1/F4/F7/G0/L4; uart_init() returns false on this family."
  (void)instance;
  (void)baud;
  s_uart_stats[instance].init_fail++;
  uart_set_error(instance, UART_ERR_INIT_FAILED);
  return false;
#endif
}

void uart_putc(uint8_t instance, uint8_t c) {
  if (!uart_valid_instance(instance)) return;
  s_uart_stats[instance].tx_attempts++;
  if (!s_uart_initialized[instance]) {
    uart_set_error(instance, UART_ERR_NOT_INITIALIZED);
    return;
  }
  uint32_t base = get_base(instance);
  if (base == 0U) {
    uart_set_error(instance, UART_ERR_BASE_UNAVAILABLE);
    return;
  }
#if defined(UART_MAP_V1) || defined(UART_MAP_V2)
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + UART_STATUS_OFFSET), ST_TXE, true, UART_TX_TIMEOUT_MS, UART_TX_TIMEOUT_LOOPS)) {
    s_uart_stats[instance].tx_timeout++;
    uart_set_error(instance, UART_ERR_TX_TIMEOUT);
    return;
  }
#if defined(UART_MAP_V1)
  UART_DR(base) = (uint32_t)c;
#else
  UART_TDR(base) = (uint32_t)c;
#endif
  s_uart_stats[instance].tx_ok++;
  uart_set_error(instance, UART_ERR_NONE);
#else
  (void)c;
#endif
}

bool uart_getc(uint8_t instance, uint8_t *c) {
  if (!uart_valid_instance(instance) || c == NULL) return false;
  s_uart_stats[instance].rx_attempts++;
  if (!s_uart_initialized[instance]) {
    uart_set_error(instance, UART_ERR_NOT_INITIALIZED);
    return false;
  }
  uint32_t base = get_base(instance);
  if (base == 0U) {
    uart_set_error(instance, UART_ERR_BASE_UNAVAILABLE);
    return false;
  }
#if defined(UART_MAP_V1)
  if ((UART_SR(base) & ST_RXNE) == 0) {
    s_uart_stats[instance].rx_empty++;
    return false;
  }
  *c = (uint8_t)UART_DR(base);
  s_uart_stats[instance].rx_ok++;
  uart_set_error(instance, UART_ERR_NONE);
  return true;
#elif defined(UART_MAP_V2)
  if ((UART_ISR(base) & ST_RXNE) == 0) {
    s_uart_stats[instance].rx_empty++;
    return false;
  }
  *c = (uint8_t)UART_RDR(base);
  s_uart_stats[instance].rx_ok++;
  uart_set_error(instance, UART_ERR_NONE);
  return true;
#else
  *c = 0;
  return false;
#endif
}

bool uart_get_stats(uint8_t instance, uart_stats_t *out_stats) {
  if (!uart_valid_instance(instance) || out_stats == NULL) return false;
  *out_stats = s_uart_stats[instance];
  return true;
}

bool uart_reset_stats(uint8_t instance) {
  if (!uart_valid_instance(instance)) return false;
  s_uart_stats[instance].init_ok = 0U;
  s_uart_stats[instance].init_fail = 0U;
  s_uart_stats[instance].tx_attempts = 0U;
  s_uart_stats[instance].tx_ok = 0U;
  s_uart_stats[instance].tx_timeout = 0U;
  s_uart_stats[instance].rx_attempts = 0U;
  s_uart_stats[instance].rx_ok = 0U;
  s_uart_stats[instance].rx_empty = 0U;
  s_uart_stats[instance].last_error = UART_ERR_NONE;
  return true;
}
