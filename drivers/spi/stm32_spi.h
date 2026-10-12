/**
 * @file stm32_spi.h
 * @brief STM32 SPI master – public API. See drivers/README.md for supported families.
 */
#ifndef STM32_SPI_H
#define STM32_SPI_H
#include <stdint.h>
#include <stdbool.h>

#ifndef SPI_TIMEOUT_MS
#define SPI_TIMEOUT_MS 50
#endif

/* Bits 0/1 select physically present SPI1/SPI2 on your exact part. */
#ifndef SPI_INSTANCE_MASK
#define SPI_INSTANCE_MASK 3U
#endif
#ifndef SPI_TIMEOUT_LOOPS
#define SPI_TIMEOUT_LOOPS 1000000U
#endif

typedef enum {
  SPI_INSTANCE_1 = 0,
  SPI_INSTANCE_2,
  SPI_INSTANCE_COUNT
} spi_instance_t;

/* Mode 0, 8-bit, MSB first. Caller configures AF pins and drives GPIO CS.
 * clock_hz is a maximum; reject values below the slowest /256 divider.
 * Override SPI_PCLK1_HZ/PCLK2_HZ when changing system clocks.
 * After a transfer failure, release CS and call spi_init before retrying. */
bool spi_init(spi_instance_t instance, uint32_t clock_hz);
bool spi_transfer(spi_instance_t instance, const uint8_t *tx, uint8_t *rx, uint16_t len);
void spi_deinit(spi_instance_t instance);

#endif /* STM32_SPI_H */
