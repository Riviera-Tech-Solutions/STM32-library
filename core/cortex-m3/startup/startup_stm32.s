;*******************************************************************************
; STM32 Cortex-M3 startup – minimal placeholder (F1, F2, L1)
; Replace with your MCU-specific startup.
;*******************************************************************************
                PRESERVE8
                THUMB
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
__Vectors       DCD     0
                END
