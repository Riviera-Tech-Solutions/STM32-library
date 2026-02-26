/**
 * @file system_stm32l4.c
 * @brief STM32L4 system init – MSI/HSI, vector table
 */
#include <stdint.h>
#define RCC_BASE    0x40021000U
#define RCC_CR      (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_CFGR    (*(volatile uint32_t *)(RCC_BASE + 0x0CU))
#define SCB_VTOR    (*(volatile uint32_t *)0xE000ED08U)
#define SCB_CPACR   (*(volatile uint32_t *)0xE000ED88U)
#define RCC_CR_MSION  (1U << 0)
#define RCC_CR_MSIRDY (1U << 1)
#define CPACR_FPU    (3U << 20)
#define VECTOR_TABLE_OFFSET 0x08000000U

void SystemInit(void) {
  SCB_CPACR |= CPACR_FPU;
  SCB_VTOR = VECTOR_TABLE_OFFSET;
  RCC_CR |= RCC_CR_MSION;
  while ((RCC_CR & RCC_CR_MSIRDY) == 0U) { }
  RCC_CFGR = (RCC_CFGR & ~(3U << 0)) | (0U << 0);
}
