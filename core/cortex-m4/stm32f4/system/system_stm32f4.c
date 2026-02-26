/**
 * @file system_stm32f4.c
 * @brief STM32F4 system init – FPU, vector table, HSI/PLL to SYSCLK
 * Senior design: config in one place; no magic numbers.
 */

#include <stdint.h>

/* -----------------------------------------------------------------------------
 * Config – override in your project or keep defaults
 * ----------------------------------------------------------------------------- */
#ifndef SYSCLK_MHZ
#define SYSCLK_MHZ         (84U)
#endif
#ifndef HSE_MHZ
#define HSE_MHZ             (0U)   /* 0 = use HSI only */
#endif

/* Register addresses (F4 reference manual) */
#define RCC_BASE           0x40023800U
#define RCC_CR              (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_CFGR            (*(volatile uint32_t *)(RCC_BASE + 0x08U))
#define RCC_AHB1ENR         (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB1ENR         (*(volatile uint32_t *)(RCC_BASE + 0x40U))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x44U))

#define SCB_BASE            0xE000ED00U
#define SCB_CPACR            (*(volatile uint32_t *)(SCB_BASE + 0x88U))
#define SCB_VTOR             (*(volatile uint32_t *)(SCB_BASE + 0x08U))

#define FLASH_ACR            (*(volatile uint32_t *)0x40023C00U)

#define RCC_CR_HSION         (1U << 0)
#define RCC_CR_HSIRDY        (1U << 1)
#define RCC_CR_PLLON         (1U << 24)
#define RCC_CR_PLLRDY        (1U << 25)
#define RCC_CFGR_SW_MASK     (3U << 0)
#define RCC_CFGR_SW_HSI     (0U << 0)
#define RCC_CFGR_SW_PLL     (2U << 0)
#define RCC_CFGR_PLLSRC_HSI (1U << 22)
#define RCC_CFGR_PLLM_MASK  (0x3FU << 4)
#define RCC_CFGR_PLLN_MASK  (0x1FFU << 6)
#define RCC_CFGR_PLLP_MASK  (3U << 16)
#define RCC_CFGR_HPRE_DIV1  (0U << 4)
#define RCC_CFGR_HPRE_MASK  (15U << 4)
#define RCC_CFGR_PPRE1_MASK (7U << 10)
#define RCC_CFGR_PPRE2_MASK (7U << 13)
#define FLASH_ACR_LATENCY   (7U << 0)
#define FLASH_ACR_PRFTEN   (1U << 8)
#define CPACR_CP10_11_FULL  (3U << 20)

#define HSI_MHZ              16U
#define VECTOR_TABLE_OFFSET  0x08000000U

static void SystemClock_Config(void) {
  /* Enable FPU (Cortex-M4) */
  SCB_CPACR |= CPACR_CP10_11_FULL;

  /* Vector table at flash base */
  SCB_VTOR = VECTOR_TABLE_OFFSET;

  /* Flash latency for SYSCLK up to 84 MHz (2 wait states) */
  FLASH_ACR = (2U << 0) | FLASH_ACR_PRFTEN;

  RCC_CR |= RCC_CR_HSION;
  while ((RCC_CR & RCC_CR_HSIRDY) == 0U) { }

  RCC_CFGR &= ~(RCC_CFGR_PLLM_MASK | RCC_CFGR_PLLN_MASK | RCC_CFGR_PLLP_MASK);
  RCC_CFGR &= ~(1U << 22);
  RCC_CFGR |= (1U << 22); /* PLLSRC = HSI */
  /* PLL: HSI/8 * 84 / 2 = 84 MHz; PLLM=8, PLLN=84, PLLP=0 means /2 */
  RCC_CFGR |= (8U << 4) | (84U << 6) | (0U << 16);
  RCC_CR |= RCC_CR_PLLON;
  while ((RCC_CR & RCC_CR_PLLRDY) == 0U) { }

  RCC_CFGR &= ~(RCC_CFGR_HPRE_MASK | RCC_CFGR_PPRE1_MASK | RCC_CFGR_PPRE2_MASK);
  RCC_CFGR |= RCC_CFGR_HPRE_DIV1 | (4U << 10) | (2U << 13); /* AHB=84, APB1=42, APB2=84 */
  RCC_CFGR = (RCC_CFGR & ~RCC_CFGR_SW_MASK) | RCC_CFGR_SW_PLL;
  while ((RCC_CFGR & (3U << 2)) != (2U << 2)) { }
}

void SystemInit(void) {
  SystemClock_Config();
}
