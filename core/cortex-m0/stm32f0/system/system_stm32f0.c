/** STM32F0 system init – HSI, VTOR */
#include <stdint.h>
#define RCC_CR      (*(volatile uint32_t *)0x40021000U)
#define SCB_VTOR    (*(volatile uint32_t *)0xE000ED08U)
#define RCC_CR_HSION  (1U << 0)
#define RCC_CR_HSIRDY (1U << 1)
void SystemInit(void) {
  SCB_VTOR = 0x08000000U;
  RCC_CR |= RCC_CR_HSION;
  while ((RCC_CR & RCC_CR_HSIRDY) == 0U) { }
}
