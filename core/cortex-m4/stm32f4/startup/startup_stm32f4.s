/******************************************************************************
 * STM32F4 (Cortex-M4) startup — GNU assembler (arm-none-eabi-as).
 *
 * Responsibilities, in order, on reset:
 *   1. Point the stack at _estack.
 *   2. Copy initialized data (.data) from FLASH to RAM.
 *   3. Zero the .bss segment.
 *   4. Call SystemInit() to bring up clocks.
 *   5. Call main().
 *   6. If main() ever returns, sit in a safe loop (an enabled IWDG will then
 *      reset the part — see drivers/wdg).
 *
 * This replaces the previous Keil/armasm version, which the GNU toolchain the
 * project ships (arm-none-eabi-gcc) cannot assemble, and which never
 * initialized the C runtime (so globals were garbage and .bss was not zeroed).
 *
 * Every exception/IRQ handler is a .weak alias of Default_Handler, so
 * application code can override any of them simply by defining a function of
 * the same name — no edits here required.
 ******************************************************************************/

  .syntax unified
  .cpu cortex-m4
  .fpu softvfp
  .thumb

/* Symbols provided by the linker script (linker.ld). */
  .word _sidata      /* .data init values, in FLASH            */
  .word _sdata       /* .data start, in RAM                    */
  .word _edata       /* .data end, in RAM                      */
  .word _sbss        /* .bss start, in RAM                     */
  .word _ebss        /* .bss end, in RAM                       */

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack           /* explicit; the core also loads SP from vec[0] */

  /* --- copy .data (FLASH -> RAM) --- */
  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  movs  r3, #0
  b     .Lcopy_check
.Lcopy_loop:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4
.Lcopy_check:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   .Lcopy_loop

  /* --- zero .bss --- */
  ldr   r2, =_sbss
  ldr   r4, =_ebss
  movs  r3, #0
  b     .Lbss_check
.Lbss_loop:
  str   r3, [r2]
  adds  r2, r2, #4
.Lbss_check:
  cmp   r2, r4
  bcc   .Lbss_loop

  /* --- clocks, then application --- */
  bl    SystemInit
  bl    main

.Lhang:
  b     .Lhang                 /* main() should never return; IWDG backstops it */
  .size Reset_Handler, .-Reset_Handler

/*
 * Default_Handler: a tight loop for any unhandled exception/IRQ. It is
 * intentionally overridable — for field recovery, define HardFault_Handler in
 * application code to log and call NVIC_SystemReset(), and/or run the IWDG so a
 * stuck handler is reset automatically.
 */
  .section .text.Default_Handler,"ax",%progbits
  .type Default_Handler, %function
Default_Handler:
.Ldefault:
  b     .Ldefault
  .size Default_Handler, .-Default_Handler

/******************************************************************************
 * Vector table. A compact table: the 15 Cortex-M core exceptions plus a
 * generous block of external-IRQ slots pointing at Default_Handler. Override
 * any specific IRQ by defining a same-named weak symbol in your code.
 ******************************************************************************/
  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
  .global g_pfnVectors
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler

  /* 96 external interrupt slots (covers STM32F4 IRQ0..IRQ95). */
  .rept 96
  .word Default_Handler
  .endr
  .size g_pfnVectors, .-g_pfnVectors

/* Weak aliases: every handler defaults to Default_Handler unless overridden. */
  .macro def_handler name
  .weak \name
  .thumb_set \name, Default_Handler
  .endm

  def_handler NMI_Handler
  def_handler HardFault_Handler
  def_handler MemManage_Handler
  def_handler BusFault_Handler
  def_handler UsageFault_Handler
  def_handler SVC_Handler
  def_handler DebugMon_Handler
  def_handler PendSV_Handler
  def_handler SysTick_Handler
