/**
 * @file stm32_adc.c
 * @brief STM32 ADC (single-channel polled). IMPLEMENTED: STM32F0, STM32F1,
 *        STM32F4, STM32F7, STM32G0, STM32L4. Other families are NOT
 *        implemented — adc_init() returns false and a #warning fires at
 *        build time.
 *
 * Three register generations, verified against ST CMSIS device headers:
 *   SAR v1 (SR/CR2/SQR3/DR, ADON+SWSTART):    F1 (with mandatory CAL),
 *                                             F4, F7
 *   SAR v2 (ISR/CR/CHSELR/DR, ADCAL+ADEN):    F0, G0
 *   SAR v3 (ISR/CR/SQR1/DR, DEEPPWD+ADVREGEN+ADCAL+ADEN): L4
 *
 * Each init runs the family's REQUIRED bring-up sequence (regulator, then
 * calibration with ADEN=0, then enable and wait ADRDY) — skipping these
 * "works" in a simulator and returns garbage or hangs on silicon.
 */
#include "stm32_adc.h"
#include "../common/stm32_family.h"
#include <stddef.h>
#include "../common/stm32_wait.h"

#ifndef ADC_TIMEOUT_LOOPS
#define ADC_TIMEOUT_LOOPS 1000000U
#endif

/* Crude post-regulator-enable settle (spec ~20 us): loop count sized for the
   fastest clock this library configures (84 MHz F4) so it is safe everywhere. */
#ifndef ADC_VREG_SETTLE_LOOPS
#define ADC_VREG_SETTLE_LOOPS 4000U
#endif

#define ADC_INSTANCE_COUNT 1U

#if defined(STM32F4)
#define ADC_MAP_V1
#define ADC1_BASE    0x40012000U
#define ADC_RCC_EN() ((*(volatile uint32_t *)(0x40023844U)) |= (1U << 8))  /* APB2ENR ADC1EN */

#elif defined(STM32F7)
#define ADC_MAP_V1
#define ADC1_BASE    0x40012000U
#define ADC_RCC_EN() ((*(volatile uint32_t *)(0x40023844U)) |= (1U << 8))  /* APB2ENR ADC1EN */

#elif defined(STM32F1)
#define ADC_MAP_V1
#define ADC_NEEDS_CAL
#define ADC1_BASE    0x40012400U
#define ADC_RCC_EN() ((*(volatile uint32_t *)(0x40021018U)) |= (1U << 9))  /* APB2ENR ADC1EN */

#elif defined(STM32F0)
#define ADC_MAP_V2
#define ADC1_BASE    0x40012400U
#define ADC_RCC_EN() ((*(volatile uint32_t *)(0x40021018U)) |= (1U << 9))  /* APB2ENR ADCEN */
/* F0 has no ADVREGEN bit; regulator is implicit. */

#elif defined(STM32G0)
#define ADC_MAP_V2
#define ADC_HAS_VREG
#define ADC1_BASE    0x40012400U
/* RM0444/CMSIS: APBENR2 @ RCC+0x40, ADCEN is bit 20. (A previous revision
   wrote bit 9 of reserved space at 0x40021058 — the ADC clock never enabled.) */
#define ADC_RCC_EN() ((*(volatile uint32_t *)(0x40021040U)) |= (1U << 20)) /* APBENR2 ADCEN */

#elif defined(STM32L4) || defined(STM32L4PLUS)
#define ADC_MAP_V3
#define ADC1_BASE    0x50040000U
#define ADC_COMMON_CCR (*(volatile uint32_t *)(0x50040308U))               /* ADC12_COMMON + 0x08 */
#define ADC_RCC_EN() ((*(volatile uint32_t *)(0x4002104CU)) |= (1U << 13)) /* AHB2ENR ADCEN */
#endif

/* ---- Register maps ------------------------------------------------------ */
#if defined(ADC_MAP_V1)
#define ADC_SR(b)    (*(volatile uint32_t *)((b) + 0x00U))
#define ADC_CR2(b)   (*(volatile uint32_t *)((b) + 0x08U))
#define ADC_SQR3(b)  (*(volatile uint32_t *)((b) + 0x34U))
#define ADC_DR(b)    (*(volatile uint32_t *)((b) + 0x4CU))
#define CR2_ADON     (1U << 0)
#define CR2_CAL      (1U << 2)
#define CR2_EXTSEL_SW (7U << 17)
#define CR2_EXTTRIG  (1U << 20)
#define CR2_SWSTART  (1U << 22)
#define SR_EOC       (1U << 1)
#if defined(STM32F4) || defined(STM32F7)
#undef  CR2_SWSTART
#define CR2_SWSTART  (1U << 30)
#endif
#elif defined(ADC_MAP_V2) || defined(ADC_MAP_V3)
#define ADC_ISR(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define ADC_CR(b)    (*(volatile uint32_t *)((b) + 0x08U))
#define ADC_CFGR2(b) (*(volatile uint32_t *)((b) + 0x10U))
#define CR_ADEN      (1U << 0)
#define CR_ADSTART   (1U << 2)
#define CR_ADVREGEN  (1U << 28)
#define CR_DEEPPWD   (1U << 29)
#define CR_ADCAL     (1U << 31)
#define ISR_ADRDY    (1U << 0)
#define ISR_EOC      (1U << 2)
#if defined(ADC_MAP_V2)
#define ADC_CHSELR(b) (*(volatile uint32_t *)((b) + 0x28U))
#define ADC_DR(b)    (*(volatile uint32_t *)((b) + 0x40U))
#define CFGR2_CKMODE_PCLK2 (1U << 30)   /* CKMODE = 01: PCLK/2, no HSI14/async dependency */
#else /* ADC_MAP_V3 */
#define ADC_SQR1(b)  (*(volatile uint32_t *)((b) + 0x30U))
#define ADC_DR(b)    (*(volatile uint32_t *)((b) + 0x40U))
#define CCR_CKMODE_HCLK1 (1U << 16)     /* CKMODE = 01: HCLK/1 (CCIPR default gives NO async clock) */
#endif
#endif

static uint32_t get_base(uint8_t instance) {
#if defined(ADC_MAP_V1) || defined(ADC_MAP_V2) || defined(ADC_MAP_V3)
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

#if defined(ADC_MAP_V1) || defined(ADC_MAP_V2) || defined(ADC_MAP_V3)
static void adc_settle_delay(void) {
  for (volatile uint32_t d = 0; d < ADC_VREG_SETTLE_LOOPS; d++) { (void)d; }
}
#endif

bool adc_init(uint8_t instance) {
  if (!adc_valid_instance(instance)) return false;
  s_adc_initialized[instance] = false;
#if defined(ADC_MAP_V1) || defined(ADC_MAP_V2) || defined(ADC_MAP_V3)
  if (instance != 0) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_INVALID_INSTANCE);
    return false;
  }
  if (ADC_INPUT_CLOCK_HZ == 0U || ADC_INPUT_CLOCK_HZ > ADC_MAX_INPUT_CLOCK_HZ) goto init_timeout;
  ADC_RCC_EN();
  uint32_t base = get_base(instance);
  if (base == 0U) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_BASE_UNAVAILABLE);
    return false;
  }

#if defined(ADC_MAP_V2) || defined(ADC_MAP_V3)
  if (ADC_CR(base) & CR_ADSTART) {
    ADC_CR(base) |= (1U << 4); /* ADSTP */
    if (!stm32_wait_mask_eq32(&ADC_CR(base), CR_ADSTART | (1U << 4), false, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) goto init_timeout;
  }
  if (ADC_CR(base) & CR_ADEN) {
    ADC_CR(base) |= (1U << 1); /* ADDIS */
    if (!stm32_wait_mask_eq32(&ADC_CR(base), CR_ADEN | (1U << 1), false, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) goto init_timeout;
  }
  if (!stm32_wait_mask_eq32(&ADC_CR(base), CR_ADCAL, false, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) goto init_timeout;
  *(volatile uint32_t *)(base + 0x0CU) = 0U; /* single, software-triggered, 12 bit */
#if defined(ADC_MAP_V2)
  ADC_CFGR2(base) = (2U << 30); /* synchronous PCLK/4 */
  *(volatile uint32_t *)(base + 0x14U) = 7U;
#if defined(STM32G0)
  *(volatile uint32_t *)(base + 0x14U) = 0x77U;
#endif
#else
  *(volatile uint32_t *)(base + 0x14U) = 0x3FFFFFFFU;
  *(volatile uint32_t *)(base + 0x18U) = 0x07FFFFFFU;
#endif
#endif

#if defined(ADC_MAP_V1)
  ADC_CR2(base) = 0U; /* Own regular ADC1 configuration; stop before setup. */
#if defined(STM32F1)
  /* APB2 / 6: <=12 MHz even at the family maximum 72 MHz. */
  volatile uint32_t *cfgr = (volatile uint32_t *)0x40021004U;
  *cfgr = (*cfgr & ~(3U << 14)) | (2U << 14);
#else
  /* APB2 / 4: 21 MHz at this library's F4 default (84 MHz APB2). */
  volatile uint32_t *ccr = (volatile uint32_t *)0x40012304U;
  *ccr = (*ccr & ~(3U << 16)) | (1U << 16);
#endif
  *(volatile uint32_t *)(base + 0x04U) = 0U; /* 12-bit, no scan/interrupts */
  *(volatile uint32_t *)(base + 0x2CU) = 0U; /* one regular conversion */
  *(volatile uint32_t *)(base + 0x0CU) = 0x00FFFFFFU; /* long sample times */
  *(volatile uint32_t *)(base + 0x10U) = 0x3FFFFFFFU;
  ADC_CR2(base) = CR2_ADON;
  adc_settle_delay(); /* tSTAB before first conversion, on F4/F7 too */
#if defined(ADC_NEEDS_CAL)
  adc_settle_delay();                 /* tSTAB before calibration */
  ADC_CR2(base) |= (1U << 3); /* Reset calibration before calibrating. */
  if (!stm32_wait_mask_eq32(&ADC_CR2(base), 1U << 3, false, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) goto init_timeout;
  ADC_CR2(base) |= CR2_CAL;
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + 0x08U), CR2_CAL, false, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return false;
  }
#endif
  /* Route software start: F1 needs EXTSEL=SWSTART + EXTTRIG for SWSTART to fire. */
#if defined(STM32F1)
  ADC_CR2(base) |= CR2_EXTSEL_SW | CR2_EXTTRIG;
#endif

#elif defined(ADC_MAP_V2)
  /* PCLK/4 configured above, before calibration. */
#if defined(ADC_HAS_VREG)
  ADC_CR(base) |= CR_ADVREGEN;
  adc_settle_delay();
#endif
  ADC_CR(base) |= CR_ADCAL;           /* calibrate with ADEN = 0 */
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + 0x08U), CR_ADCAL, false, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return false;
  }
  adc_settle_delay(); /* Delay between calibration and ADEN. */
  ADC_ISR(base) = ISR_ADRDY;          /* rc_w1: clear stale ready flag */
  ADC_CR(base) |= CR_ADEN;
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + 0x00U), ISR_ADRDY, true, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return false;
  }

#elif defined(ADC_MAP_V3)
  ADC_COMMON_CCR = (ADC_COMMON_CCR & ~(3U << 16)) | (3U << 16); /* HCLK/4 */
  ADC_CR(base) &= ~CR_DEEPPWD;        /* exit deep power-down */
  ADC_CR(base) |= CR_ADVREGEN;
  adc_settle_delay();
  ADC_CR(base) |= CR_ADCAL;           /* calibrate with ADEN = 0 */
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + 0x08U), CR_ADCAL, false, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return false;
  }
  adc_settle_delay(); /* Delay between calibration and ADEN. */
  ADC_ISR(base) = ISR_ADRDY;
  ADC_CR(base) |= CR_ADEN;
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + 0x00U), ISR_ADRDY, true, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].init_fail++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return false;
  }
#endif

  s_adc_initialized[instance] = true;
  s_adc_stats[instance].init_ok++;
  adc_set_error(instance, ADC_ERR_NONE);
  return true;
init_timeout:
  s_adc_stats[instance].init_fail++;
  s_adc_stats[instance].timeout_count++;
  adc_set_error(instance, ADC_ERR_TIMEOUT);
  return false;
#else
#warning "stm32_adc: implemented only for STM32F0/F1/F4/F7/G0/L4; adc_init() returns false on this family."
  (void)instance;
  s_adc_stats[0].init_fail++;
  adc_set_error(0, ADC_ERR_INIT_FAILED);
  return false;
#endif
}

uint16_t adc_read(uint8_t instance, uint8_t channel) {
  if (!adc_valid_instance(instance)) return 0U;
  s_adc_stats[instance].read_attempts++;
  if (channel > 18U || !(ADC_CHANNEL_MASK & (1UL << channel))) {
    s_adc_stats[instance].read_fail++;
    adc_set_error(instance, ADC_ERR_INVALID_CHANNEL);
    return 0U;
  }
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
#if defined(ADC_MAP_V1)
  ADC_SR(base) = 0U; /* discard stale completion after a prior failure */
  ADC_SQR3(base) = (uint32_t)channel;
  ADC_CR2(base) |= CR2_SWSTART;
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + 0x00U), SR_EOC, true, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].read_fail++;
    s_adc_initialized[instance] = false;
    s_adc_stats[instance].timeout_count++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return 0U;
  }
  s_adc_stats[instance].read_ok++;
  adc_set_error(instance, ADC_ERR_NONE);
  return (uint16_t)(ADC_DR(base) & 0xFFFU);
#elif defined(ADC_MAP_V2)
  if (channel > 18U) {
    s_adc_stats[instance].read_fail++;
    adc_set_error(instance, ADC_ERR_INVALID_CHANNEL);
    return 0U;
  }
  ADC_ISR(base) = ISR_EOC | (1U << 3) | (1U << 4);
#if defined(STM32G0)
  ADC_ISR(base) = (1U << 13); /* CCRDY, W1C */
#endif
  ADC_CHSELR(base) = (1U << channel);
#if defined(STM32G0)
  if (!stm32_wait_mask_eq32(&ADC_ISR(base), 1U << 13, true, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_initialized[instance] = false;
    s_adc_stats[instance].read_fail++;
    s_adc_initialized[instance] = false;
    s_adc_stats[instance].timeout_count++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return 0U;
  }
#endif
  ADC_CR(base) |= CR_ADSTART;
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + 0x00U), ISR_EOC, true, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].read_fail++;
    s_adc_initialized[instance] = false;
    s_adc_stats[instance].timeout_count++;
    adc_set_error(instance, ADC_ERR_TIMEOUT);
    return 0U;
  }
  s_adc_stats[instance].read_ok++;
  adc_set_error(instance, ADC_ERR_NONE);
  return (uint16_t)(ADC_DR(base) & 0xFFFU);
#elif defined(ADC_MAP_V3)
  if (channel > 18U) {
    s_adc_stats[instance].read_fail++;
    adc_set_error(instance, ADC_ERR_INVALID_CHANNEL);
    return 0U;
  }
  ADC_ISR(base) = ISR_EOC | (1U << 3) | (1U << 4);
  ADC_SQR1(base) = ((uint32_t)channel & 0x1FU) << 6;   /* L = 0: one conversion */
  ADC_CR(base) |= CR_ADSTART;
  if (!stm32_wait_mask_eq32((volatile uint32_t *)(base + 0x00U), ISR_EOC, true, ADC_TIMEOUT_MS, ADC_TIMEOUT_LOOPS)) {
    s_adc_stats[instance].read_fail++;
    s_adc_initialized[instance] = false;
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
  s_adc_stats[instance].read_fail = 0U;
  s_adc_stats[instance].read_ok = 0U;
  s_adc_stats[instance].timeout_count = 0U;
  s_adc_stats[instance].last_error = ADC_ERR_NONE;
  return true;
}
