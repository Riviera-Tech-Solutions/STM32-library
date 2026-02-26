;*******************************************************************************
; STM32 Cortex-M4 startup – minimal placeholder for code suite
; Replace with your MCU-specific startup (vector table, Reset_Handler, Stack/Heap).
;*******************************************************************************
                PRESERVE8
                THUMB
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
__Vectors       DCD     0
                END
