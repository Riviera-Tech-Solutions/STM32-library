#ifndef STM32_WAIT_H
#define STM32_WAIT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Optional system millisecond tick hook.
 * Define STM32_WAIT_GET_TICK_MS() in your build to use real timeout in ms.
 * If not defined, the helper falls back to bounded spin loops.
 */
#ifndef STM32_WAIT_GET_TICK_MS
#define STM32_WAIT_GET_TICK_MS() 0U
#endif

#ifndef STM32_WAIT_USE_TICK
#define STM32_WAIT_USE_TICK 0
#endif

static inline bool stm32_wait_mask_eq32(volatile uint32_t *reg,
                                        uint32_t mask,
                                        bool set,
                                        uint32_t timeout_ms,
                                        uint32_t spin_limit)
{
#if STM32_WAIT_USE_TICK
  uint32_t start = STM32_WAIT_GET_TICK_MS();
  while ((((*reg) & mask) != 0U) != set) {
    if ((uint32_t)(STM32_WAIT_GET_TICK_MS() - start) >= timeout_ms) {
      return false;
    }
  }
  return true;
#else
  uint32_t remaining = spin_limit;
  while ((((*reg) & mask) != 0U) != set) {
    if (remaining-- == 0U) {
      return false;
    }
  }
  (void)timeout_ms;
  return true;
#endif
}

#endif /* STM32_WAIT_H */
