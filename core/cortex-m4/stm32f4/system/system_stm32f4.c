/**
 * @file system_stm32f4.c
 * @brief STM32F4 system init — FPU, vector table, HSI→PLL→SYSCLK bring-up.
 *
 * Default: HSI (16 MHz) → PLL → 84 MHz SYSCLK, which is valid on every F4 part
 * (F401 max is 84 MHz; higher parts can go faster — raise SYSCLK_MHZ and the
 * flash wait-states scale automatically). Override SYSCLK_MHZ at compile time.
 *
 * Fixes vs the previous version, which could never lock the PLL: the F4 PLL
 * configuration lives in RCC_PLLCFGR (offset 0x04), NOT RCC_CFGR (0x08). The
 * old code wrote PLLM/PLLN/PLLP/PLLSRC into RCC_CFGR, scrambling the bus
 * prescalers and leaving the PLL unconfigured.
 *
 * NOTE: register layout is verified against RM0368/RM0090; on-device clock lock
 * still needs validation on your specific board (HSE vs HSI, exact part).
 */

#include <stdint.h>

/* ---- User config (override in your build) ---------------------------------- */
#ifndef SYSCLK_MHZ
#define SYSCLK_MHZ          (84U)
#endif
#ifndef HSI_MHZ
#define HSI_MHZ             (16U)   /* STM32F4 internal RC oscillator */
#endif

/* ---- RCC ------------------------------------------------------------------- */
#define RCC_BASE            0x40023800U
#define RCC_CR              (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_PLLCFGR         (*(volatile uint32_t *)(RCC_BASE + 0x04U))  /* PLL config lives HERE */
#define RCC_CFGR            (*(volatile uint32_t *)(RCC_BASE + 0x08U))

#define RCC_CR_HSION        (1U << 0)
#define RCC_CR_HSIRDY       (1U << 1)
#define RCC_CR_PLLON        (1U << 24)
#define RCC_CR_PLLRDY       (1U << 25)

#define RCC_PLLCFGR_PLLSRC_HSI (0U << 22)   /* PLLSRC bit 22: 0 = HSI, 1 = HSE */

#define RCC_CFGR_SW_MASK    (3U << 0)
#define RCC_CFGR_SW_PLL     (2U << 0)
#define RCC_CFGR_SWS_MASK   (3U << 2)
#define RCC_CFGR_SWS_PLL    (2U << 2)
#define RCC_CFGR_HPRE_DIV1  (0U << 4)
#define RCC_CFGR_HPRE_MASK  (0xFU << 4)
#define RCC_CFGR_PPRE1_MASK (7U << 10)
#define RCC_CFGR_PPRE1_DIV2 (4U << 10)      /* APB1 = HCLK/2 (max 42 MHz on F401) */
#define RCC_CFGR_PPRE2_MASK (7U << 13)
#define RCC_CFGR_PPRE2_DIV1 (0U << 13)      /* APB2 = HCLK */

/* ---- FLASH / SCB ----------------------------------------------------------- */
#define FLASH_ACR           (*(volatile uint32_t *)0x40023C00U)
#define FLASH_ACR_PRFTEN    (1U << 8)
#define FLASH_ACR_ICEN      (1U << 9)
#define FLASH_ACR_DCEN      (1U << 10)

#define SCB_CPACR           (*(volatile uint32_t *)0xE000ED88U)
#define SCB_VTOR            (*(volatile uint32_t *)0xE000ED08U)
#define CPACR_CP10_11_FULL  (0xFU << 20)
#define VECTOR_TABLE_BASE   0x08000000U

/* ---- Derived PLL parameters -----------------------------------------------
 * VCO input is fixed at 2 MHz (recommended): PLLM = HSI_MHZ / 2.
 * With PLLP = /2, SYSCLK = (2 MHz * PLLN) / 2 = PLLN MHz, so PLLN = SYSCLK_MHZ.
 * ------------------------------------------------------------------------- */
#define PLL_VCO_IN_MHZ      (2U)
#define PLLM                (HSI_MHZ / PLL_VCO_IN_MHZ)   /* 16/2 = 8   */
#define PLLN                (SYSCLK_MHZ)                 /* 84         */
#define PLLP_DIV            (2U)                         /* SYSCLK = VCO/2 */
#define PLLP_BITS           (0U)                         /* 00 encodes /2 */

/* Flash wait-states: F4 @ 2.7–3.6 V needs +1 WS per ~30 MHz. */
#define FLASH_WS            ((SYSCLK_MHZ - 1U) / 30U)

static void SystemClock_Config(void) {
  /* Enable the FPU (CP10/CP11 full access) before any float use. */
  SCB_CPACR |= CPACR_CP10_11_FULL;
  __asm volatile ("dsb");
  __asm volatile ("isb");

  /* Relocate the vector table to flash base. */
  SCB_VTOR = VECTOR_TABLE_BASE;

  /* Flash: set wait-states + enable caches/prefetch BEFORE raising the clock. */
  FLASH_ACR = (FLASH_WS & 0xFU) | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

  /* Make sure HSI is on and stable (it is the PLL source). */
  RCC_CR |= RCC_CR_HSION;
  while ((RCC_CR & RCC_CR_HSIRDY) == 0U) { }

  /* Configure the PLL in RCC_PLLCFGR:
   *   PLLM  bits [5:0], PLLN bits [14:6], PLLP bits [17:16], PLLSRC bit 22. */
  RCC_PLLCFGR =
      ((PLLM & 0x3FU) << 0) |
      ((PLLN & 0x1FFU) << 6) |
      ((PLLP_BITS & 0x3U) << 16) |
      RCC_PLLCFGR_PLLSRC_HSI;

  /* Start the PLL and wait for lock. */
  RCC_CR |= RCC_CR_PLLON;
  while ((RCC_CR & RCC_CR_PLLRDY) == 0U) { }

  /* Bus prescalers: AHB = SYSCLK, APB1 = /2, APB2 = /1. */
  RCC_CFGR = (RCC_CFGR & ~(RCC_CFGR_HPRE_MASK | RCC_CFGR_PPRE1_MASK | RCC_CFGR_PPRE2_MASK)) |
             RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;

  /* Switch SYSCLK to the PLL and wait until it is actually selected. */
  RCC_CFGR = (RCC_CFGR & ~RCC_CFGR_SW_MASK) | RCC_CFGR_SW_PLL;
  while ((RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) { }
}

void SystemInit(void) {
  SystemClock_Config();
}
