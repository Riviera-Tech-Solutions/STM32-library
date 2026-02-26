;*******************************************************************************
; STM32F4 Cortex-M4 startup – vector table, Reset_Handler, stack/heap
; Senior design: single file per family; no magic numbers (equates below).
;*******************************************************************************
                PRESERVE8
                THUMB
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors       DCD     __initial_sp
                DCD     Reset_Handler
                DCD     NMI_Handler
                DCD     HardFault_Handler
                DCD     MemManage_Handler
                DCD     BusFault_Handler
                DCD     UsageFault_Handler
                DCD     0
                DCD     0
                DCD     0
                DCD     0
                DCD     SVC_Handler
                DCD     DebugMon_Handler
                DCD     0
                DCD     PendSV_Handler
                DCD     SysTick_Handler
                ; External interrupts – extend as needed for your part
                SPACE   16 * 4
__Vectors_End

__Vectors_Size  EQU     __Vectors_End - __Vectors

                AREA    |.text|, CODE, READONLY
Reset_Handler   PROC
                EXPORT  Reset_Handler
                IMPORT  SystemInit
                IMPORT  main
                LDR     R0, =SystemInit
                BLX     R0
                LDR     R0, =main
                BX      R0
                ENDP

NMI_Handler     PROC
                EXPORT  NMI_Handler
                B       .
                ENDP
HardFault_Handler PROC
                EXPORT  HardFault_Handler
                B       .
                ENDP
MemManage_Handler PROC
                EXPORT  MemManage_Handler
                B       .
                ENDP
BusFault_Handler PROC
                EXPORT  BusFault_Handler
                B       .
                ENDP
UsageFault_Handler PROC
                EXPORT  UsageFault_Handler
                B       .
                ENDP
SVC_Handler     PROC
                EXPORT  SVC_Handler
                B       .
                ENDP
DebugMon_Handler PROC
                EXPORT  DebugMon_Handler
                B       .
                ENDP
PendSV_Handler  PROC
                EXPORT  PendSV_Handler
                B       .
                ENDP
SysTick_Handler PROC
                EXPORT  SysTick_Handler
                B       .
                ENDP

                ALIGN
                END
