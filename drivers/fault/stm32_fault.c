/**
 * @file stm32_fault.c
 * @brief Fault handlers that record the crash and reset (see stm32_fault.h).
 *
 * Portable across ARMv6-M (Cortex-M0/M0+, which has ONLY a HardFault and no
 * fault-status registers) and ARMv7/8-M (M3/M4/M7/M33, which also have
 * MemManage/BusFault/UsageFault and CFSR/HFSR).
 */
#include "stm32_fault.h"
#include "../common/stm32_family.h"

/* Cortex-M system reset via SCB->AIRCR. */
#define SCB_AIRCR         (*(volatile uint32_t *)0xE000ED0CU)
#define AIRCR_VECTKEY     0x05FA0000U
#define AIRCR_SYSRESETREQ (1U << 2)

#define FAULT_MAGIC       0xFA017ECCU

/* Both live in .noinit so they survive the reset the handler triggers. The
   magic word is kept separate from the public record so the API stays clean. */
static volatile fault_record_t s_fault __attribute__((section(".noinit")));
static volatile uint32_t s_fault_magic __attribute__((section(".noinit")));

/*
 * Common C fault entry. @p sp points at the exception stack frame the core
 * pushed: [r0 r1 r2 r3 r12 lr pc xpsr]. Record the essentials, then reset.
 */
__attribute__((used))
static void fault_handler_c(uint32_t *sp) {
  if (s_fault_magic != FAULT_MAGIC) {
    s_fault_magic = FAULT_MAGIC;
    s_fault.count = 0U;
  }
  s_fault.count++;
  s_fault.lr  = sp[5];
  s_fault.pc  = sp[6];
  s_fault.psr = sp[7];
#if !defined(__ARM_ARCH_6M__)
  /* CFSR/HFSR exist only on ARMv7-M and above. */
  s_fault.cfsr = *(volatile uint32_t *)0xE000ED28U;
  s_fault.hfsr = *(volatile uint32_t *)0xE000ED2CU;
#else
  s_fault.cfsr = 0U;
  s_fault.hfsr = 0U;
#endif

  __asm volatile ("dsb");
  SCB_AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
  __asm volatile ("dsb");
  for (;;) { }   /* wait for reset */
}

/*
 * Naked trampoline: pick the stack (MSP vs PSP per EXC_RETURN bit 2) and jump
 * to the C handler. Uses only ARMv6-M-legal instructions — no IT blocks — so
 * it assembles on Cortex-M0 as well as the bigger cores.
 */
#define FAULT_TRAMPOLINE(name)                       \
  __attribute__((naked)) void name(void) {           \
    __asm volatile (                                 \
      "movs r0, #4            \n"                     \
      "mov  r1, lr           \n"                     \
      "tst  r0, r1           \n"                     \
      "beq  1f               \n"                     \
      "mrs  r0, psp          \n"                     \
      "b    2f               \n"                     \
      "1:                    \n"                     \
      "mrs  r0, msp          \n"                     \
      "2:                    \n"                     \
      "ldr  r1, =fault_handler_c \n"                 \
      "bx   r1               \n"                     \
    );                                               \
  }

FAULT_TRAMPOLINE(HardFault_Handler)
#if !defined(__ARM_ARCH_6M__)
FAULT_TRAMPOLINE(MemManage_Handler)
FAULT_TRAMPOLINE(BusFault_Handler)
FAULT_TRAMPOLINE(UsageFault_Handler)
#endif

bool fault_get_last(fault_record_t *out) {
  if (out == 0) return false;
  if (s_fault_magic != FAULT_MAGIC || s_fault.count == 0U) return false;
  *out = s_fault;
  return true;
}

void fault_clear(void) {
  s_fault_magic = FAULT_MAGIC;
  s_fault.count = 0U;
}
