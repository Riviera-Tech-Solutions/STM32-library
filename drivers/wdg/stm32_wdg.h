/**
 * @file stm32_wdg.h
 * @brief STM32 Independent Watchdog (IWDG) — public API.
 *
 * The IWDG runs off the low-speed internal RC (LSI, ~32 kHz), independent of the
 * main clock and CPU. Once started it CANNOT be stopped in software — if your
 * code stops calling wdg_kick() (a hang, a fault handler that spins, a stuck
 * peripheral loop), the chip resets automatically. This is the single most
 * important building block for a device that must run unattended for years.
 *
 * See the support matrix for mapped families and reset-cause availability.
 * Override WDG_LSI_HZ for the actual oscillator frequency.
 *
 * Typical use:
 *   wdg_init(2000);            // reset if not kicked within ~2 s
 *   for (;;) {
 *     do_work();
 *     wdg_kick();              // "I'm still alive"
 *   }
 *
 * LSI is an RC oscillator with wide tolerance (roughly ±30–50%), so the timeout
 * is approximate — size it generously (2–4x your worst-case loop time), never
 * tight. It is a safety backstop, not a precise timer.
 */
#ifndef STM32_WDG_H
#define STM32_WDG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the independent watchdog for approximately @p timeout_ms milliseconds.
 * Once started it runs until the next reset — there is no stop. Call wdg_kick()
 * more often than @p timeout_ms or the device resets.
 *
 * @param timeout_ms desired timeout in ms (clamped to the achievable range,
 *                   roughly 1 ms .. ~32000 ms at the nominal 32 kHz LSI).
 * @return the actual timeout the hardware was programmed to, in ms (may differ
 *         from the request due to prescaler/reload granularity). 0 on error.
 *         A synchronization timeout does NOT stop an already-started watchdog.
 */
uint32_t wdg_init(uint32_t timeout_ms);

/** Reload the watchdog counter ("kick"/"feed"/"pet"). Call this periodically. */
void wdg_kick(void);

/** True if the LAST reset was caused by the independent watchdog firing.
 *  Reads and clears the RCC reset flags. Useful for logging field resets. */
bool wdg_caused_last_reset(void);

/** Immediately reset the whole system via the Cortex-M AIRCR. Handy to call
 *  from a HardFault handler so a fatal fault recovers instead of hanging. */
void wdg_system_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32_WDG_H */
