/* GNU assembler startup for Cortex-m0plus. Cold reset, C (no constructors).
 * Polled-driver template: external IRQ slots are Default_Handler. Replace with
 * the exact device's CMSIS vector table before enabling peripheral interrupts.
 * M33 requires board-specific TrustZone/security setup before nonsecure entry. */
.syntax unified
.cpu cortex-m0plus
.thumb
.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
  ldr r0, =_estack
  mov sp, r0
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
1:
  cmp r0, r1
  bcs 2f
  ldr r3, [r2]
  str r3, [r0]
  adds r0, #4
  adds r2, #4
  b 1b
2:
  ldr r0, =_sbss
  ldr r1, =_ebss
  movs r2, #0
3:
  cmp r0, r1
  bcs 4f
  str r2, [r0]
  adds r0, #4
  b 3b
4:
  bl SystemInit
  bl main
5:
  b 5b
.size Reset_Handler, .-Reset_Handler
.section .text.Default_Handler,"ax",%progbits
.type Default_Handler, %function
.thumb_func
Default_Handler:
  b .
.size Default_Handler, .-Default_Handler
.section .isr_vector,"a",%progbits
.balign 512
.global g_pfnVectors
g_pfnVectors:
  .word _estack, Reset_Handler, NMI_Handler, HardFault_Handler
  .word 0, 0, 0
  .word 0, 0, 0, 0, SVC_Handler
  .word 0, 0, PendSV_Handler, SysTick_Handler
  .rept 240
  .word Default_Handler
  .endr
.macro def_handler name
  .weak \name
  .thumb_set \name, Default_Handler
.endm
def_handler NMI_Handler
def_handler HardFault_Handler
def_handler SVC_Handler
def_handler PendSV_Handler
def_handler SysTick_Handler
