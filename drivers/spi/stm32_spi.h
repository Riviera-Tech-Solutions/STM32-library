/**
 * @file stm32_spi.h
 * @brief STM32 SPI master – public API. Define STM32F4 or STM32G0 for implementation.
 */
#ifndef STM32_SPI_H
#define STM32_SPI_H
#include <stdint.h>
#include <stdbool.h>

#ifndef SPI_TIMEOUT_MS
#define SPI_TIMEOUT_MS 50
#endif

typedef enum {
  SPI_INSTANCE_1 = 0,
  SPI_INSTANCE_2,
  SPI_INSTANCE_COUNT
} spi_instance_t;

bool spi_init(spi_instance_t instance, uint32_t clock_hz);
bool spi_transfer(spi_instance_t instance, const uint8_t *tx, uint8_t *rx, uint16_t len);
void spi_deinit(spi_instance_t instance);

#endif /* STM32_SPI_H */
