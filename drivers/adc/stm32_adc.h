/** @file stm32_adc.h – ADC driver API (stub). */
#ifndef STM32_ADC_H
#define STM32_ADC_H
#include <stdint.h>
#include <stdbool.h>
/* Poll timeout guard for adc_read() wait loops. */
#ifndef ADC_TIMEOUT_LOOPS
#define ADC_TIMEOUT_LOOPS 1000000U
#endif

typedef enum {
  ADC_ERR_NONE = 0,
  ADC_ERR_INVALID_INSTANCE,
  ADC_ERR_INVALID_CHANNEL,
  ADC_ERR_NOT_INITIALIZED,
  ADC_ERR_BASE_UNAVAILABLE,
  ADC_ERR_TIMEOUT,
  ADC_ERR_INIT_FAILED
} adc_error_t;

typedef struct {
  uint32_t init_ok;
  uint32_t init_fail;
  uint32_t read_attempts;
  uint32_t read_ok;
  uint32_t read_fail;
  uint32_t timeout_count;
  adc_error_t last_error;
} adc_stats_t;

bool adc_init(uint8_t instance);
uint16_t adc_read(uint8_t instance, uint8_t channel);
bool adc_get_stats(uint8_t instance, adc_stats_t *out_stats);
bool adc_reset_stats(uint8_t instance);
#endif
