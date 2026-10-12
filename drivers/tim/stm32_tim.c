/**
 * @file stm32_tim.c
 * @brief STM32 Timer / PWM (TIM2/TIM3, channel 1). IMPLEMENTED: STM32F0,
 *        STM32F1, STM32F4, STM32F7, STM32G0, STM32L4. Other families are NOT
 *        implemented — tim_pwm_init() returns false and a #warning fires at
 *        build time.
 *
 * TIM2/TIM3 share one register layout on every supported family (CMSIS-
 * verified): CR1@0x00 CCMR1@0x18 CCER@0x20 PSC@0x28 ARR@0x2C CCR1@0x34.
 * Only the RCC enable register and the timer input clock differ.
 *
 * Timer clock defaults to what THIS library's SystemInit actually leaves
 * running. Note the F4 subtlety: APB1 runs at 42 MHz (HCLK/2), and per the
 * clock-tree rule an APB prescaler != 1 feeds timers 2x the APB clock, so
 * TIM2/TIM3 clock at 84 MHz. Families left on HSI/MSI (prescaler = 1) clock
 * timers at PCLK. Override TIM_CLK_HZ if you reconfigure the clock tree.
 *
 * Instance map: 0 = TIM2, 1 = TIM3. Hardware gaps to know about:
 *   - STM32F030 parts have no TIM2 (F070/F072 do) — instance 0 will program
 *     a void address on F030.
 *   - STM32L412/L432 have no TIM3 (L476 etc. do) — instance 1 likewise.
 * The library cannot see your exact part number at compile time; check the
 * datasheet before relying on a second timer instance.
 */
#include "stm32_tim.h"
#include "../common/stm32_family.h"

#if defined(STM32F4)
#define TIM_RCC_EN   (*(volatile uint32_t *)(0x40023840U))   /* APB1ENR */
#define TIM2_EN_BIT  (1U << 0)
#define TIM3_EN_BIT  (1U << 1)
#ifndef TIM_CLK_HZ
#define TIM_CLK_HZ   84000000U   /* APB1 42 MHz x2 (APB prescaler = /2) */
#endif

#elif defined(STM32F7)
#define TIM_RCC_EN   (*(volatile uint32_t *)(0x40023840U))   /* APB1ENR */
#define TIM2_EN_BIT  (1U << 0)
#define TIM3_EN_BIT  (1U << 1)
#ifndef TIM_CLK_HZ
#define TIM_CLK_HZ   16000000U   /* HSI, APB prescaler = /1 */
#endif

#elif defined(STM32F1)
#define TIM_RCC_EN   (*(volatile uint32_t *)(0x4002101CU))   /* APB1ENR */
#define TIM2_EN_BIT  (1U << 0)
#define TIM3_EN_BIT  (1U << 1)
#ifndef TIM_CLK_HZ
#define TIM_CLK_HZ   8000000U    /* HSI */
#endif

#elif defined(STM32F0)
#define TIM_RCC_EN   (*(volatile uint32_t *)(0x4002101CU))   /* APB1ENR */
#define TIM2_EN_BIT  (1U << 0)
#define TIM3_EN_BIT  (1U << 1)
#ifndef TIM_CLK_HZ
#define TIM_CLK_HZ   8000000U    /* HSI */
#endif

#elif defined(STM32G0)
/* RM0444/CMSIS: APBENR1 @ RCC+0x3C. (A previous revision wrote 0x40021058 —
   reserved space — so the timer clock never enabled.) */
#define TIM_RCC_EN   (*(volatile uint32_t *)(0x4002103CU))   /* APBENR1 */
#define TIM2_EN_BIT  (1U << 0)
#define TIM3_EN_BIT  (1U << 1)
#ifndef TIM_CLK_HZ
#define TIM_CLK_HZ   16000000U   /* HSI16; SystemInit does not start the PLL */
#endif

#elif defined(STM32L4) || defined(STM32L4PLUS)
#define TIM_RCC_EN   (*(volatile uint32_t *)(0x40021058U))   /* APB1ENR1 */
#define TIM2_EN_BIT  (1U << 0)
#define TIM3_EN_BIT  (1U << 1)
#ifndef TIM_CLK_HZ
#define TIM_CLK_HZ   4000000U    /* MSI default */
#endif
#endif

#if defined(TIM_RCC_EN)
#define TIM_IMPLEMENTED 1
#define TIM2_BASE    0x40000000U
#define TIM3_BASE    0x40000400U
#define TIM_CR1(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define TIM_CCMR1(b) (*(volatile uint32_t *)((b) + 0x18U))
#define TIM_CCER(b)  (*(volatile uint32_t *)((b) + 0x20U))
#define TIM_EGR(b)   (*(volatile uint32_t *)((b) + 0x14U))
#define TIM_SR(b)    (*(volatile uint32_t *)((b) + 0x10U))
#define TIM_CNT(b)   (*(volatile uint32_t *)((b) + 0x24U))
#define TIM_PSC(b)   (*(volatile uint32_t *)((b) + 0x28U))
#define TIM_ARR(b)   (*(volatile uint32_t *)((b) + 0x2CU))
#define TIM_CCR1(b)  (*(volatile uint32_t *)((b) + 0x34U))
#define CR1_CEN      (1U << 0)
#define CCMR1_OC1M_PWM1 (6U << 4)
#define CCMR1_OC1PE  (1U << 3)
#define CCER_CC1E    (1U << 0)
#endif

static uint32_t get_base(uint8_t instance) __attribute__((unused));
static uint32_t get_base(uint8_t instance) {
#if defined(TIM_IMPLEMENTED)
  return (instance == 0) ? TIM2_BASE : TIM3_BASE;
#else
  (void)instance;
  return 0;
#endif
}

bool tim_pwm_init(uint8_t instance, uint8_t channel, uint32_t freq_hz) {
  (void)channel;
#if defined(TIM_IMPLEMENTED)
  if (instance > 1U || channel != 1U || !(TIM_INSTANCE_MASK & (1U << instance)) || freq_hz == 0U || freq_hz > TIM_CLK_HZ) return false;
  TIM_RCC_EN |= (instance == 0) ? TIM2_EN_BIT : TIM3_EN_BIT;
  uint32_t base = get_base(instance);
  uint32_t target = (freq_hz > 0U) ? freq_hz : 1000U;
  /* Pick the smallest prescaler that fits the period in 16 bits, so low
     frequencies work even from a fast timer clock. */
  uint32_t psc = 1U;
  uint32_t arr = TIM_CLK_HZ / target;
  while (arr > 65535U && psc < 65536U) {
    psc <<= 1;
    arr = (TIM_CLK_HZ / psc) / target;
  }
  if (arr == 0U) arr = 1U;
  if (arr > 65535U) return false;
  TIM_CR1(base) = 0;
  TIM_PSC(base) = psc - 1U;
  TIM_ARR(base) = arr - 1U;
  TIM_CCR1(base) = 0;
  TIM_CCMR1(base) = CCMR1_OC1M_PWM1 | CCMR1_OC1PE;
  TIM_CCER(base) = CCER_CC1E;
  TIM_EGR(base) = 1U; /* UG: load PSC and preloaded CCR before first period. */
  TIM_SR(base) = 0U;
  TIM_CNT(base) = 0U;
  TIM_CR1(base) = CR1_CEN;
  return true;
#else
#warning "stm32_tim: PWM is implemented only for STM32F0/F1/F4/F7/G0/L4; tim_pwm_init() returns false on this family."
  /* Not implemented for this family — fail honestly. Silently returning true
     here would report a working PWM that never toggles a pin. */
  (void)instance;
  (void)freq_hz;
  return false;
#endif
}

void tim_pwm_set_duty(uint8_t instance, uint8_t channel, uint8_t percent) {
  (void)channel;
#if defined(TIM_IMPLEMENTED)
  if (instance > 1U || channel != 1U || !(TIM_INSTANCE_MASK & (1U << instance))) return;
  uint32_t base = get_base(instance);
  if (percent > 100U) percent = 100U;
  uint32_t arr = TIM_ARR(base) + 1U;
  uint32_t ccr = (arr * (uint32_t)percent) / 100U;
  if (ccr > arr) ccr = arr;
  TIM_CCR1(base) = ccr;
#else
  (void)instance;
  (void)percent;
#endif
}
