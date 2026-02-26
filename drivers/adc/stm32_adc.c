/**
 * @file stm32_adc.c
 * @brief STM32 ADC – F4 and G0 single-channel polled. Other families: stub. Define STM32F4 or STM32G0.
 */
#include "stm32_adc.h"
#include "../common/stm32_wait.h"

#ifndef ADC_TIMEOUT_LOOPS
#define ADC_TIMEOUT_LOOPS 1000000U
#endif

#define ADC_INSTANCE_COUNT 1U

#if defined(STM32F4)
#define ADC1_BASE    0x40012000U
#define RCC_APB2ENR  (*(volatile uint32_t *)(0x40023844U))
#define ADC_CR2(b)   (*(volatile uint32_t *)((b) + 0x08U))
#define ADC_SR(b)    (*(volatile uint32_t *)((b) + 0x00U))
#define ADC_SQR3(b)  (*(volatile uint32_t *)((b) + 0x34U))
#define ADC_DR(b)    (*(volatile uint32_t *)((b) + 0x4CU))
#define CR2_ADON     (1U << 0)
#define CR2_SWSTART  (1U << 30)
#define SR_EOC       (1U << 1)
#endif

#if defined(STM32G0)
#define ADC1_BASE    0x40012400U
#define RCC_APBENR2  (*(volatile uint32_t *)(0x40021058U))
#define ADC_CR(b)    (*(volatile uint32_t *)((b) + 0x08U))
#define ADC_ISR(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define ADC_CHSELR(b) ((*(volatile uint32_t *)((b) + 0x28U)))
#define ADC_DR(b)    (*(volatile uint32_t *)((b) + 0x40U))
#define CR_ADEN      (1U << 0)
#define CR_ADSTART   (1U << 2)
#define ISR_EOC     (1U << 2)
#endif

static uint32_t get_base(uint8_t instance) {
#if defined(STM32F4) || defined(STM32G0)
  return (instance == 0) ? ADC1_BASE : 0;
#else
  (void)instance;
  return 0;
#endif
}

static bool s_adc_initialized[ADC_INSTANCE_COUNT];
static adc_stats_t s_adc_stats[ADC_INSTANCE_COUNT];

static bool adc_valid_instance(uint8_t instance) {
  return instance < ADC_INSTANCE_COUNT;
}

static void adc_set_error(uint8_t instance, adc_error_t err) {
  if (!adc_valid_instance(instance)) return;
  s_adc_stats[instance].last_error = err;
}

bool adc_init(uint8_t instance) {
  if (!adc_valid_instance(instance)) return false;
#if defined(STM32F4)
  if (instance != 0) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_INVALID_INSTANCE);
    return false;
  }
  RCC_APB2ENR |= (1U << 8);
  uint32_t base = get_base(instance);
  if (base == 0U) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_BASE_UNAVAILABLE);
    return false;
  }
  ADC_CR2(base) |= CR2_ADON;
  s_adc_initialized[instance] = true;
  s_adc_stats[instance].init_ok++;
  adc_set_error(instance, ADC_ERR_NONE);
  return true;
#elif defined(STM32G0)
  if (instance != 0) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_INVALID_INSTANCE);
    return false;
  }
  RCC_APBENR2 |= (1U << 9);
  uint32_t base = get_base(instance);
  if (base == 0U) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_BASE_UNAVAILABLE);
    return false;
  }
  ADC_CR(base) |= CR_ADEN;
  s_adc_initialized[instance] = true;
  s_adc_stats[instance].init_ok++;
  adc_set_error(instance, ADC_ERR_NONE);
  return true;
#else
  (void)instance;
  s_adc_stats[0].init_fail++;
  adc_set_error(0, ADC_ERR_INIT_FAILED);
  return false;
#endif
}

uint16_t adc_read(uint8_t instance, uint8_t channel) {
  if (!adc_valid_instance(instance)) return 0U;
  s_adc_stats[instance].read_attempts++;
  if (!s_adc_initialized[instance]) {
    s_adc_stats[instance].read_fail++;
    adc_set_error(instance, ADC_ERR_NOT_INITIALIZED);
    return 0U;
  }
  uint32_t base = get_base(instance);
  if (base == 0U) {
    s_adc_stats[instance].read_fail++;
    adc_set_error(instance, ADC_ERR_BASE_UNAVAILABLE);
    return 0U;
  }
#if defined(STM32F4)
  ADC_SQR3(base) = (uint32_t)channel & 0x1FU;
  ADC_CR2(base) |= CR2_SWSTART;
  if (!stm32_wait_mask_eq32((volatile uint32_t *)((base) + 0x00U), SR_EOC, true, ADC_TIMEOUT_LOOPS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].read_fail++;
    s_adc_stats[instance].timeout_count++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return 0U;
  }
  s_adc_stats[instance].read_ok++;
  adc_set_error(instance, ADC_ERR_NONE);
  return (uint16_t)(ADC_DR(base) & 0xFFFU);
#elif defined(STM32G0)
  if (channel > 18U) {
    s_adc_stats[instance].read_fail++;
    adc_set_error(instance, ADC_ERR_INVALID_CHANNEL);
    return 0U;
  }
  ADC_CHSELR(base) = (1U << channel);
  ADC_CR(base) |= CR_ADSTART;
  if (!stm32_wait_mask_eq32((volatile uint32_t *)((base) + 0x00U), ISR_EOC, true, ADC_TIMEOUT_LOOPS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].read_fail++;
    s_adc_stats[instance].timeout_count++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return 0U;
  }
  s_adc_stats[instance].read_ok++;
  adc_set_error(instance, ADC_ERR_NONE);
  return (uint16_t)(ADC_DR(base) & 0xFFFU);
#else
  (void)channel;
  (void)base;
  s_adc_stats[instance].read_fail++;
  adc_set_error(instance, ADC_ERR_INIT_FAILED);
  return 0U;
#endif
}

bool adc_get_stats(uint8_t instance, adc_stats_t *out_stats) {
  if (!adc_valid_instance(instance) || out_stats == NULL) return false;
  *out_stats = s_adc_stats[instance];
  return true;
}

bool adc_reset_stats(uint8_t instance) {
  if (!adc_valid_instance(instance)) return false;
  s_adc_stats[instance].init_ok = 0U;
  s_adc_stats[instance].init_fail = 0U;
  s_adc_stats[instance].read_attempts = 0U;
  s_adc_stats[instance].read_ok = 0U;
  s_adc_stats[instance].read_fail = 0U;
  s_adc_stats[instance].timeout_count = 0U;
  s_adc_stats[instance].last_error = ADC_ERR_NONE;
  return true;
}
