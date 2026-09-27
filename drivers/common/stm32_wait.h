#ifndef STM32_WAIT_H
#define STM32_WAIT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Optional system millisecond tick hook.
 * Define STM32_WAIT_GET_TICK_MS() in your build to use real timeout in ms.
 * If not defined, the helper falls back to bounded spin loops.
 */
#ifndef STM32_WAIT_USE_TICK
#define STM32_WAIT_USE_TICK 0
#endif

#if STM32_WAIT_USE_TICK && !defined(STM32_WAIT_GET_TICK_MS)
#error "Tick waits require STM32_WAIT_GET_TICK_MS()"
#endif

static inline bool stm32_wait_mask_eq32(volatile uint32_t *reg,
                                        uint32_t mask,
                                        bool set,
                                        uint32_t timeout_ms,
                                        uint32_t spin_limit)
{
  uint32_t remaining = spin_limit;
#if STM32_WAIT_USE_TICK
  uint32_t start = STM32_WAIT_GET_TICK_MS();
#else
  (void)timeout_ms;
#endif
  while ((((*reg) & mask) != 0U) != set) {
    /* Remains bounded even before SysTick starts or with interrupts masked. */
    if (remaining == 0U) return false;
    --remaining;
#if STM32_WAIT_USE_TICK
    if ((uint32_t)(STM32_WAIT_GET_TICK_MS() - start) >= timeout_ms) {
      return false;
    }
#endif
  }
  return true;
}

#endif /* STM32_WAIT_H */
