/** STM32F3 system init – HSI, VTOR, FPU */
#include <stdint.h>
#define SCB_CPACR  (*(volatile uint32_t *)0xE000ED88U)
#define SCB_VTOR   (*(volatile uint32_t *)0xE000ED08U)
#define RCC_CR     (*(volatile uint32_t *)0x40021000U)
void SystemInit(void) {
  SCB_CPACR |= (3U << 20);
  SCB_VTOR = 0x08000000U;
  RCC_CR |= (1U << 0);
  while ((RCC_CR & (1U << 1)) == 0U) { }
}
