/* Cold-reset clock template: leave the factory reset oscillator/prescalers
 * unchanged. Application bootloader handoff, PLLs and TrustZone setup require
 * an exact-device SystemInit override and matching driver clock defines.
 * Avoid guessed RCC addresses/bits across incompatible device families. */
#include <stdint.h>
__attribute__((weak)) void SystemInit(void) {
  /* Reset memory alias supplies vectors; no assumed VTOR on v6-M. */

}
