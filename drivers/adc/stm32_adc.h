/** @file stm32_adc.h – Single-channel polled ADC1 API. */
#ifndef STM32_ADC_H
#define STM32_ADC_H
#include <stdint.h>
#include <stdbool.h>
/* Poll timeout guard for adc_read() wait loops. */
#ifndef ADC_TIMEOUT_LOOPS
#define ADC_TIMEOUT_LOOPS 1000000U
#endif

#ifndef ADC_TIMEOUT_MS
#define ADC_TIMEOUT_MS 100U
#endif
/* Input is APB2 (F1/F4/F7), PCLK (F0/G0), HCLK (L4). */
#ifndef ADC_INPUT_CLOCK_HZ
#if defined(STM32F4)
#define ADC_INPUT_CLOCK_HZ 84000000U
#elif defined(STM32F0) || defined(STM32F1)
#define ADC_INPUT_CLOCK_HZ 8000000U
#elif defined(STM32L4) || defined(STM32L4PLUS)
#define ADC_INPUT_CLOCK_HZ 4000000U
#else
#define ADC_INPUT_CLOCK_HZ 16000000U
#endif
#endif
#if defined(STM32F1)
#define ADC_MAX_INPUT_CLOCK_HZ 72000000U
#elif defined(STM32F0)
#define ADC_MAX_INPUT_CLOCK_HZ 48000000U
#elif defined(STM32G0)
#define ADC_MAX_INPUT_CLOCK_HZ 64000000U
#elif defined(STM32L4) || defined(STM32L4PLUS)
#define ADC_MAX_INPUT_CLOCK_HZ 120000000U
#else
#define ADC_MAX_INPUT_CLOCK_HZ 108000000U
#endif
/* External inputs only by default. Narrow to your exact part/package.
 * Internal channels require application-specific enable and settling. */
#ifndef ADC_CHANNEL_MASK
#if defined(STM32L4) || defined(STM32L4PLUS)
#define ADC_CHANNEL_MASK 0x1FFFEUL
#else
#define ADC_CHANNEL_MASK 0xFFFFUL
#endif
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

/* Owns ADC1 and its common prescaler. Caller configures analog pins.
 * Call again after a read timeout; do not share the common clock with another
 * ADC user. adc_read returns 0 on failure: inspect last_error to distinguish 0V. */
bool adc_init(uint8_t instance);
uint16_t adc_read(uint8_t instance, uint8_t channel);
bool adc_get_stats(uint8_t instance, adc_stats_t *out_stats);
bool adc_reset_stats(uint8_t instance);
#endif
