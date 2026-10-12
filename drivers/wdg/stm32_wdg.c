/**
 * @file stm32_wdg.c
 * @brief STM32 Independent Watchdog (IWDG) driver — bare-metal register access.
 *
 * IWDG key register sequence (RM, all STM32 families):
 *   0x5555 -> KR : unlock PR/RLR for writing
 *   0xAAAA -> KR : reload the counter (kick)
 *   0xCCCC -> KR : start the watchdog
 * Timeout ~= (RLR + 1) * (4 << PR) / f_LSI.
 */
#include "stm32_wdg.h"
#include "../common/stm32_family.h"
#include "../common/stm32_wait.h"

/* IWDG base — 0x40003000 on every family EXCEPT H7, where IWDG1 sits in the
 * D3 domain (CMSIS: 0x58004800). */
#if defined(STM32H7)
#define IWDG_BASE    0x58004800U
#else
#define IWDG_BASE    0x40003000U
#endif
#define IWDG_KR      (*(volatile uint32_t *)(IWDG_BASE + 0x00U))
#define IWDG_PR      (*(volatile uint32_t *)(IWDG_BASE + 0x04U))
#define IWDG_RLR     (*(volatile uint32_t *)(IWDG_BASE + 0x08U))
#define IWDG_SR      (*(volatile uint32_t *)(IWDG_BASE + 0x0CU))

#define KR_RELOAD    0xAAAAU
#define KR_UNLOCK    0x5555U
#define KR_START     0xCCCCU

#define SR_PVU       (1U << 0)   /* prescaler update in progress */
#define SR_RVU       (1U << 1)   /* reload update in progress    */

/* Nominal LSI. Real LSI varies widely; timeout is deliberately approximate. */
#ifndef WDG_LSI_HZ
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F3)
#define WDG_LSI_HZ 40000U
#else
#define WDG_LSI_HZ 32000U
#endif
#endif
#define LSI_HZ WDG_LSI_HZ
#ifndef WDG_TIMEOUT_MS
#define WDG_TIMEOUT_MS 1000U
#endif
#ifndef WDG_TIMEOUT_LOOPS
#define WDG_TIMEOUT_LOOPS 1000000U
#endif
#define RLR_MAX      0x0FFFU     /* 12-bit reload */

/* Cortex-M reset via SCB->AIRCR. */
#define SCB_AIRCR    (*(volatile uint32_t *)0xE000ED0CU)
#define AIRCR_VECTKEY 0x05FA0000U
#define AIRCR_SYSRESETREQ (1U << 2)

/* Reset-cause register — address AND bit layout are family-specific
 * (CMSIS-verified; a previous revision used F4's 0x40023874 for everyone).
 * IWDGRSTF is bit 29 on all families below except H7 (RSR, bit 26). */
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F3)
#define RCC_CSR       (*(volatile uint32_t *)(0x40021024U))
#define RCC_CSR_IWDGRSTF (1U << 29)
#define RCC_CSR_RMVF     (1U << 24)
#elif defined(STM32F2) || defined(STM32F4) || defined(STM32F7)
#define RCC_CSR       (*(volatile uint32_t *)(0x40023874U))
#define RCC_CSR_IWDGRSTF (1U << 29)
#define RCC_CSR_RMVF     (1U << 24)
#elif defined(STM32G0)
#define RCC_CSR       (*(volatile uint32_t *)(0x40021060U))
#define RCC_CSR_IWDGRSTF (1U << 29)
#define RCC_CSR_RMVF     (1U << 23)
#elif defined(STM32G4) || defined(STM32L4) || defined(STM32L4PLUS)
#define RCC_CSR       (*(volatile uint32_t *)(0x40021094U))
#define RCC_CSR_IWDGRSTF (1U << 29)
#define RCC_CSR_RMVF     (1U << 23)
#elif defined(STM32L0)
#define RCC_CSR       (*(volatile uint32_t *)(0x40021050U))
#define RCC_CSR_IWDGRSTF (1U << 29)
#define RCC_CSR_RMVF     (1U << 23)
#elif defined(STM32L1)
#define RCC_CSR       (*(volatile uint32_t *)(0x40023834U))
#define RCC_CSR_IWDGRSTF (1U << 29)
#define RCC_CSR_RMVF     (1U << 24)
#elif defined(STM32H7)
#define RCC_CSR       (*(volatile uint32_t *)(0x580244D0U))   /* RCC_RSR */
#define RCC_CSR_IWDGRSTF (1U << 26)                           /* IWDG1RSTF */
#define RCC_CSR_RMVF     (1U << 16)
#endif

uint32_t wdg_init(uint32_t timeout_ms) {
#if defined(RCC_CSR)
  if (timeout_ms == 0U) {
    timeout_ms = 1U;
  }

  /* Find the smallest prescaler whose reload fits in 12 bits, so we keep the
   * best timing resolution. PR encodes divider = 4 << PR (4,8,16,…,256). */
  uint32_t pr = 0U;
  uint32_t reload = 0U;
  for (pr = 0U; pr <= 6U; pr++) {
    const uint32_t div = 4U << pr;
    /* reload = timeout_ms * f_LSI / (1000 * div) - 1, in 64-bit to avoid overflow */
    uint64_t r = ((uint64_t)timeout_ms * LSI_HZ) / (1000ULL * div);
    if (r == 0ULL) r = 1ULL;
    if (r > 0ULL) r -= 1ULL;
    if (r <= RLR_MAX) {
      reload = (uint32_t)r;
      break;
    }
    reload = RLR_MAX;   /* keep the max if nothing smaller fits */
  }

  if (pr > 6U) pr = 6U; /* Hardware encodes both 6 and 7 as /256. */
  /* START turns on LSI before PR/RLR synchronization. It cannot be undone. */
  IWDG_KR = KR_START;
  /* Program the IWDG. */
  IWDG_KR = KR_UNLOCK;
  /* wait until PR/RLR are writable */
  if (!stm32_wait_mask_eq32(&IWDG_SR, SR_PVU | SR_RVU, false,
                           WDG_TIMEOUT_MS, WDG_TIMEOUT_LOOPS)) return 0U;
  IWDG_PR = pr;
  IWDG_RLR = reload;
  if (!stm32_wait_mask_eq32(&IWDG_SR, SR_PVU | SR_RVU, false,
                           WDG_TIMEOUT_MS, WDG_TIMEOUT_LOOPS)) return 0U;
  IWDG_KR = KR_RELOAD;   /* load the reload value */


  /* Report the actual programmed timeout in ms. */
  const uint32_t div = 4U << pr;
  return (uint32_t)(((uint64_t)(reload + 1U) * 1000ULL * div) / LSI_HZ);
#else
  (void)timeout_ms;
  return 0U; /* Family mapping not verified. */
#endif
}

void wdg_kick(void) {
#if defined(RCC_CSR)
  IWDG_KR = KR_RELOAD;
#endif
}

bool wdg_caused_last_reset(void) {
#if defined(RCC_CSR)
  const bool was_iwdg = (RCC_CSR & RCC_CSR_IWDGRSTF) != 0U;
  RCC_CSR |= RCC_CSR_RMVF;   /* clear the reset-cause flags */
  return was_iwdg;
#else
  /* Reset-cause register not mapped for this family yet (WB/WL/H5/L5/U5) —
     answer false honestly rather than read a wrong address. The watchdog
     start is also rejected until the complete family map is verified. */
  return false;
#endif
}

void wdg_system_reset(void) {
  __asm volatile ("dsb");
  SCB_AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
  __asm volatile ("dsb");
  for (;;) { }   /* wait for the reset to take effect */
}
