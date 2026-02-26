/** @file stm32_uart.h – UART driver API (stub). */
#ifndef STM32_UART_H
#define STM32_UART_H
#include <stdint.h>
#include <stdbool.h>
/* Poll timeout guard for uart_putc() wait loops. */
#ifndef UART_TX_TIMEOUT_LOOPS
#define UART_TX_TIMEOUT_LOOPS 1000000U
#endif

typedef enum {
  UART_ERR_NONE = 0,
  UART_ERR_INVALID_INSTANCE,
  UART_ERR_INVALID_PARAM,
  UART_ERR_BASE_UNAVAILABLE,
  UART_ERR_NOT_INITIALIZED,
  UART_ERR_TX_TIMEOUT,
  UART_ERR_INIT_FAILED
} uart_error_t;

typedef struct {
  uint32_t init_ok;
  uint32_t init_fail;
  uint32_t tx_attempts;
  uint32_t tx_ok;
  uint32_t tx_timeout;
  uint32_t rx_attempts;
  uint32_t rx_ok;
  uint32_t rx_empty;
  uart_error_t last_error;
} uart_stats_t;

bool uart_init(uint8_t instance, uint32_t baud);
void uart_putc(uint8_t instance, uint8_t c);
bool uart_getc(uint8_t instance, uint8_t *c);
bool uart_get_stats(uint8_t instance, uart_stats_t *out_stats);
bool uart_reset_stats(uint8_t instance);
#endif
