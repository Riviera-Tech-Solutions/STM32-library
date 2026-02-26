/**
 * FreeRTOS hook implementations for STM32 (Riviera Tech STM32 Suite)
 * Add this file to your project when using configCHECK_FOR_STACK_OVERFLOW 2
 * and/or configUSE_MALLOC_FAILED_HOOK 1 in FreeRTOSConfig.h.
 */

#include "FreeRTOS.h"
#include "task.h"

#if ( configCHECK_FOR_STACK_OVERFLOW > 0 )
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    ( void ) xTask;
    ( void ) pcTaskName;
    /* Halt or breakpoint: stack overflow in task pcTaskName. */
    taskDISABLE_INTERRUPTS();
    for ( ;; )
        ;
}
#endif

#if ( configUSE_MALLOC_FAILED_HOOK == 1 )
void vApplicationMallocFailedHook( void )
{
    /* Halt or breakpoint: heap allocation failed. Increase configTOTAL_HEAP_SIZE or reduce usage. */
    taskDISABLE_INTERRUPTS();
    for ( ;; )
        ;
}
#endif
