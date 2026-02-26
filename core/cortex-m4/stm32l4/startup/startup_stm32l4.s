; STM32L4 Cortex-M4 startup
                PRESERVE8
                THUMB
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
__Vectors       DCD     __initial_sp
                DCD     Reset_Handler
                DCD     NMI_Handler
                DCD     HardFault_Handler
                SPACE   9 * 4
                DCD     SVC_Handler
                DCD     0
                DCD     PendSV_Handler
                DCD     SysTick_Handler
                SPACE   68 * 4
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
SVC_Handler     PROC
                EXPORT  SVC_Handler
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
