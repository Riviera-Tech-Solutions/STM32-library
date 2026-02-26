/**
 * @file stm32_tim.c
 * @brief STM32 Timer (PWM) – F4 and G0 implementations. Other families: stub. Define STM32F4 or STM32G0.
 */
#include "stm32_tim.h"

#if defined(STM32F4)
#define TIM2_BASE    0x40000000U
#define TIM3_BASE    0x40000400U
#define RCC_APB1ENR  (*(volatile uint32_t *)(0x40023840U))
#define TIM_CR1(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define TIM_CCMR1(b) (*(volatile uint32_t *)((b) + 0x18U))
#define TIM_CCER(b)  (*(volatile uint32_t *)((b) + 0x20U))
#define TIM_ARR(b)   (*(volatile uint32_t *)((b) + 0x2CU))
#define TIM_CCR1(b)  (*(volatile uint32_t *)((b) + 0x34U))
#define TIM_PSC(b)   (*(volatile uint32_t *)((b) + 0x28U))
#define CR1_CEN      (1U << 0)
#define CCMR1_OC1M   (7U << 4)
#define CCMR1_OC1PE  (1U << 3)
#define CCER_CC1E    (1U << 0)
#endif

#if defined(STM32G0)
#define TIM2_BASE    0x40000000U
#define TIM3_BASE    0x40000400U
#define RCC_APBENR1  (*(volatile uint32_t *)(0x40021058U))
#define TIM_CR1(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define TIM_CCMR1(b) (*(volatile uint32_t *)((b) + 0x18U))
#define TIM_CCER(b)  (*(volatile uint32_t *)((b) + 0x20U))
#define TIM_ARR(b)   (*(volatile uint32_t *)((b) + 0x2CU))
#define TIM_CCR1(b)  (*(volatile uint32_t *)((b) + 0x34U))
#define TIM_PSC(b)   (*(volatile uint32_t *)((b) + 0x28U))
#define CR1_CEN      (1U << 0)
#define CCMR1_OC1M   (7U << 4)
#define CCER_CC1E    (1U << 0)
#endif

static uint32_t get_base(uint8_t instance) {
#if defined(STM32F4) || defined(STM32G0)
  return (instance == 0) ? TIM2_BASE : TIM3_BASE;
#else
  (void)instance;
  return 0;
#endif
}

bool tim_pwm_init(uint8_t instance, uint8_t channel, uint32_t freq_hz) {
  (void)channel;
#if defined(STM32F4)
  if (instance == 0) RCC_APB1ENR |= (1U << 0);
  else RCC_APB1ENR |= (1U << 1);
  uint32_t base = get_base(instance);
  uint32_t apb1 = 42000000U;
  uint32_t psc = 1u;
  uint32_t arr = (apb1 / psc) / (freq_hz > 0 ? freq_hz : 1000u);
  if (arr > 65535u) arr = 65535u;
  TIM_CR1(base) = 0;
  TIM_PSC(base) = psc - 1u;
  TIM_ARR(base) = arr - 1u;
  TIM_CCR1(base) = 0;
  TIM_CCMR1(base) = (6U << 4);
  TIM_CCER(base) = CCER_CC1E;
  TIM_CR1(base) = CR1_CEN;
  return true;
#elif defined(STM32G0)
  if (instance == 0) RCC_APBENR1 |= (1U << 0);
  else RCC_APBENR1 |= (1U << 1);
  uint32_t base = get_base(instance);
  uint32_t pclk = 64000000U;
  uint32_t arr = (pclk / (freq_hz > 0 ? freq_hz : 1000u));
  if (arr > 65535u) arr = 65535u;
  TIM_CR1(base) = 0;
  TIM_PSC(base) = 0;
  TIM_ARR(base) = arr - 1u;
  TIM_CCR1(base) = 0;
  TIM_CCMR1(base) = (6U << 4);
  TIM_CCER(base) = CCER_CC1E;
  TIM_CR1(base) = CR1_CEN;
  return true;
#else
  (void)instance;
  (void)freq_hz;
  return true;
#endif
}

void tim_pwm_set_duty(uint8_t instance, uint8_t channel, uint8_t percent) {
  (void)channel;
  uint32_t base = get_base(instance);
#if defined(STM32F4) || defined(STM32G0)
  uint32_t arr = TIM_ARR(base) + 1u;
  uint32_t ccr = (arr * (uint32_t)percent) / 100u;
  if (ccr > arr) ccr = arr;
  TIM_CCR1(base) = ccr;
#else
  (void)percent;
#endif
}
