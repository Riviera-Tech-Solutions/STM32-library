#ifndef STM32_IRQ_H
#define STM32_IRQ_H
#include <stdint.h>
/* Preserve the caller's interrupt state around short hardware sequences. */
static inline uint32_t stm32_irq_save(void) {
#if defined(__arm__) || defined(__thumb__)
  uint32_t state;
  __asm volatile ("mrs %0, primask\n cpsid i" : "=r" (state) :: "memory");
  return state;
#else
  return 0U; /* Host regression builds only. */
#endif
}
static inline void stm32_irq_restore(uint32_t state) {
#if defined(__arm__) || defined(__thumb__)
  __asm volatile ("msr primask, %0" :: "r" (state) : "memory");
#else
  (void)state;
#endif
}
#endif
