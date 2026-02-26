/** STM32F2 system init – HSI, VTOR */
#include <stdint.h>
#define RCC_CR   (*(volatile uint32_t *)0x40023800U)
#define SCB_VTOR (*(volatile uint32_t *)0xE000ED08U)
void SystemInit(void) {
  SCB_VTOR = 0x08000000U;
  RCC_CR |= (1U << 0);
  while ((RCC_CR & (1U << 1)) == 0U) { }
}
