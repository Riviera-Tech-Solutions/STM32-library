/**
 * @file stm32_uart.c
 * @brief STM32 UART – F4 and G0 implementations (polled). Other families: stub. Define STM32F4 or STM32G0.
 */
#include "stm32_uart.h"
#include <stddef.h>
#include "../common/stm32_wait.h"

#ifndef UART_TX_TIMEOUT_LOOPS
#define UART_TX_TIMEOUT_LOOPS 1000000U
#endif

#define UART_INSTANCE_COUNT 2U

#if defined(STM32F4)
#define USART1_BASE  0x40011000U
#define USART2_BASE  0x40004400U
#define RCC_APB2ENR  (*(volatile uint32_t *)(0x40023844U))
#define RCC_APB1ENR  (*(volatile uint32_t *)(0x40023840U))
#define UART_CR1(b)  (*(volatile uint32_t *)((b) + 0x0CU))
#define UART_CR2(b)  (*(volatile uint32_t *)((b) + 0x10U))
#define UART_BRR(b)  (*(volatile uint32_t *)((b) + 0x08U))
#define UART_SR(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define UART_DR(b)   (*(volatile uint32_t *)((b) + 0x04U))
#define CR1_UE       (1U << 13)
#define CR1_TE       (1U << 3)
#define CR1_RE       (1U << 2)
#define SR_TXE       (1U << 7)
#define SR_RXNE      (1U << 5)
#endif

#if defined(STM32G0)
#define USART1_BASE  0x40013800U
#define USART2_BASE  0x40004400U
#define RCC_APBENR2  (*(volatile uint32_t *)(0x40021058U))
#define RCC_APBENR1  (*(volatile uint32_t *)(0x4002105CU))
#define UART_CR1(b)  (*(volatile uint32_t *)((b) + 0x00U))
#define UART_BRR(b)  (*(volatile uint32_t *)((b) + 0x0CU))
#define UART_ISR(b)  (*(volatile uint32_t *)((b) + 0x1CU))
#define UART_TDR(b)  (*(volatile uint32_t *)((b) + 0x28U))
#define UART_RDR(b)  (*(volatile uint32_t *)((b) + 0x24U))
#define CR1_UE       (1U << 0)
#define CR1_TE       (1U << 3)
#define CR1_RE       (1U << 2)
#define ISR_TXE      (1U << 7)
#define ISR_RXNE     (1U << 5)
#endif

static uint32_t get_base(uint8_t instance) {
#if defined(STM32F4)
  return (instance == 0) ? USART1_BASE : USART2_BASE;
#elif defined(STM32G0)
  return (instance == 0) ? USART1_BASE : USART2_BASE;
#else
  (void)instance;
  return 0;
#endif
}

static bool s_uart_initialized[UART_INSTANCE_COUNT];
static uart_stats_t s_uart_stats[UART_INSTANCE_COUNT];

static bool uart_valid_instance(uint8_t instance) {
  return instance < UART_INSTANCE_COUNT;
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
#if defined(STM32F4)
  if (instance > 1U) {
    s_uart_stats[instance].init_fail++;
    uart_set_error(instance, UART_ERR_INVALID_INSTANCE);
    return false;
  }
  if (instance == 0) RCC_APB2ENR |= (1U << 4);
  else RCC_APB1ENR |= (1U << 17);
  uint32_t base = get_base(instance);
  if (base == 0U) {
    s_uart_stats[instance].init_fail++;
    uart_set_error(instance, UART_ERR_BASE_UNAVAILABLE);
    return false;
  }
  UART_CR1(base) &= ~CR1_UE;
  uint32_t apb_clk = 42000000U;
  UART_BRR(base) = apb_clk / baud;
  UART_CR1(base) |= CR1_TE | CR1_RE | CR1_UE;
  s_uart_initialized[instance] = true;
  s_uart_stats[instance].init_ok++;
  uart_set_error(instance, UART_ERR_NONE);
  return true;
#elif defined(STM32G0)
  if (instance > 1U) {
    s_uart_stats[instance].init_fail++;
    uart_set_error(instance, UART_ERR_INVALID_INSTANCE);
    return false;
  }
  if (instance == 0) RCC_APBENR2 |= (1U << 14);
  else RCC_APBENR1 |= (1U << 17);
  uint32_t base = get_base(instance);
  if (base == 0U) {
    s_uart_stats[instance].init_fail++;
    uart_set_error(instance, UART_ERR_BASE_UNAVAILABLE);
    return false;
  }
  UART_CR1(base) &= ~CR1_UE;
  uint32_t pclk = 64000000U;
  UART_BRR(base) = pclk / baud;
  UART_CR1(base) |= CR1_TE | CR1_RE | CR1_UE;
  s_uart_initialized[instance] = true;
  s_uart_stats[instance].init_ok++;
  uart_set_error(instance, UART_ERR_NONE);
  return true;
#else
  (void)instance;
  (void)baud;
  s_uart_stats[0].init_fail++;
  uart_set_error(0, UART_ERR_INIT_FAILED);
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
#if defined(STM32F4)
  if (!stm32_wait_mask_eq32((volatile uint32_t *)((base) + 0x00U), SR_TXE, true, UART_TX_TIMEOUT_LOOPS, UART_TX_TIMEOUT_LOOPS)) {
    s_uart_stats[instance].tx_timeout++;
    uart_set_error(instance, UART_ERR_TX_TIMEOUT);
    return;
  }
  UART_DR(base) = (uint32_t)c;
  s_uart_stats[instance].tx_ok++;
  uart_set_error(instance, UART_ERR_NONE);
#elif defined(STM32G0)
  if (!stm32_wait_mask_eq32((volatile uint32_t *)((base) + 0x1CU), ISR_TXE, true, UART_TX_TIMEOUT_LOOPS, UART_TX_TIMEOUT_LOOPS)) {
    s_uart_stats[instance].tx_timeout++;
    uart_set_error(instance, UART_ERR_TX_TIMEOUT);
    return;
  }
  UART_TDR(base) = (uint32_t)c;
  s_uart_stats[instance].tx_ok++;
  uart_set_error(instance, UART_ERR_NONE);
#else
  (void)instance;
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
#if defined(STM32F4)
  if ((UART_SR(base) & SR_RXNE) == 0) {
    s_uart_stats[instance].rx_empty++;
    return false;
  }
  *c = (uint8_t)UART_DR(base);
  s_uart_stats[instance].rx_ok++;
  uart_set_error(instance, UART_ERR_NONE);
  return true;
#elif defined(STM32G0)
  if ((UART_ISR(base) & ISR_RXNE) == 0) {
    s_uart_stats[instance].rx_empty++;
    return false;
  }
  *c = (uint8_t)UART_RDR(base);
  s_uart_stats[instance].rx_ok++;
  uart_set_error(instance, UART_ERR_NONE);
  return true;
#else
  (void)instance;
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
