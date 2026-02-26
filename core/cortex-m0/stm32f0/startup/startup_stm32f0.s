; STM32F0 Cortex-M0 startup
                THUMB
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
__Vectors       DCD     __initial_sp
                DCD     Reset_Handler
                DCD     NMI_Handler
                DCD     HardFault_Handler
                SPACE   52 * 4
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
NMI_Handler     EXPORT  NMI_Handler
                B       .
HardFault_Handler EXPORT HardFault_Handler
                B       .
                ALIGN
                END
