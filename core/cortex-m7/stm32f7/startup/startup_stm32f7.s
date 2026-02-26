; STM32F7 Cortex-M7 startup
                PRESERVE8
                THUMB
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
__Vectors       DCD     __initial_sp
                DCD     Reset_Handler
                DCD     NMI_Handler
                DCD     HardFault_Handler
                SPACE   9*4
                DCD     SVC_Handler, 0, PendSV_Handler, SysTick_Handler
                SPACE   68*4
                AREA    |.text|, CODE, READONLY
Reset_Handler   PROC
                EXPORT  Reset_Handler
                IMPORT  SystemInit, main
                LDR     R0, =SystemInit
                BLX     R0
                LDR     R0, =main
                BX      R0
                ENDP
NMI_Handler     EXPORT  NMI_Handler
                B       .
HardFault_Handler EXPORT HardFault_Handler
                B       .
SVC_Handler     EXPORT  SVC_Handler
                B       .
PendSV_Handler  EXPORT  PendSV_Handler
                B       .
SysTick_Handler EXPORT  SysTick_Handler
                B       .
                ALIGN
                END
