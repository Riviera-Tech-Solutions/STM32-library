/* Cold-reset clock template: leave the factory reset oscillator/prescalers
 * unchanged. Application bootloader handoff, PLLs and TrustZone setup require
 * an exact-device SystemInit override and matching driver clock defines.
 * Avoid guessed RCC addresses/bits across incompatible device families. */
#include <stdint.h>
__attribute__((weak)) void SystemInit(void) {
  *(volatile uint32_t *)0xE000ED08U = 0x08000000U;

}
