/**
 * @file system_stm32g0.c
 * @brief STM32G0 system init – HSI, vector table
 * Senior design: config via defines; no magic numbers.
 */
#include <stdint.h>

#define RCC_BASE    0x40021000U
#define RCC_CR      (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_CFGR    (*(volatile uint32_t *)(RCC_BASE + 0x0CU))
#define SCB_VTOR    (*(volatile uint32_t *)0xE000ED08U)
#define RCC_CR_HSION   (1U << 0)
#define RCC_CR_HSIRDY  (1U << 1)
#define RCC_CFGR_SW_HSI (0U << 0)
#define VECTOR_TABLE_OFFSET 0x08000000U

void SystemInit(void) {
  SCB_VTOR = VECTOR_TABLE_OFFSET;
  RCC_CR |= RCC_CR_HSION;
  while ((RCC_CR & RCC_CR_HSIRDY) == 0U) { }
  RCC_CFGR = (RCC_CFGR & ~(3U << 0)) | RCC_CFGR_SW_HSI;
}
