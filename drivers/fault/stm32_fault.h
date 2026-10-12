/**
 * @file stm32_fault.h
 * @brief Fault recovery — turn a fatal CPU fault into a logged reset.
 *
 * By default the startup ships weak fault handlers that spin forever, so a
 * HardFault (bad pointer, unaligned access, stack overflow into a fault) bricks
 * the device until someone power-cycles it. Selecting this module replaces them
 * with handlers that:
 *   1. capture where the fault happened (PC, LR, PSR, and the fault-status
 *      registers on cores that have them),
 *   2. store that in a RAM region that survives the reset (.noinit),
 *   3. reset the system so it recovers on its own.
 *
 * On the next boot, call fault_get_last() to see whether the previous run
 * crashed and where — log it, blink an SOS, whatever your product needs.
 *
 * Pairs naturally with the IWDG watchdog: the watchdog catches silent hangs,
 * this catches loud faults. Together they cover "the device wedged".
 */
#ifndef STM32_FAULT_H
#define STM32_FAULT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t count;   /* how many faults since the record was last cleared */
  uint32_t pc;      /* instruction that faulted            */
  uint32_t lr;      /* return address                      */
  uint32_t psr;     /* program status                      */
  uint32_t cfsr;    /* Configurable Fault Status (0 on ARMv6-M / Cortex-M0) */
  uint32_t hfsr;    /* HardFault Status  (0 on ARMv6-M)    */
} fault_record_t;

/**
 * Copy the crash record from the last run into @p out.
 * @return true if a valid fault was recorded (i.e. the previous boot crashed).
 *         Call fault_clear() after logging it.
 */
bool fault_get_last(fault_record_t *out);

/** Clear the stored fault record (call after you have logged it). */
void fault_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32_FAULT_H */
