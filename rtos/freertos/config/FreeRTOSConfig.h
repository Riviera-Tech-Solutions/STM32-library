/*
 * FreeRTOS Kernel – configuration for STM32 (Riviera Tech STM32 Suite)
 * Quick start: set configCPU_CLOCK_HZ below to your SYSCLK in Hz (e.g. 168000000 for 168 MHz).
 *
 * FreeRTOS Kernel (C) Real Time Engineers Ltd. https://www.FreeRTOS.org
 * SPDX-License-Identifier: MIT
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Your system clock in Hz (required). e.g. 168000000 (F4 @ 168 MHz), 72000000 (F1 @ 72 MHz). */
#define configCPU_CLOCK_HZ    ( ( unsigned long ) 168000000 )

#define configTICK_RATE_HZ                         ( ( TickType_t ) 1000 )
#define configUSE_PREEMPTION                        1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    1
#define configUSE_TICKLESS_IDLE                     0
#define configMAX_PRIORITIES                        8
#define configMINIMAL_STACK_SIZE                    ( ( uint16_t ) 128 )
#define configMAX_TASK_NAME_LEN                     16
#define configTICK_TYPE_WIDTH_IN_BITS              TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                     1
#define configUSE_TASK_NOTIFICATIONS               1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES       1
#define configQUEUE_REGISTRY_SIZE                   0
#define configUSE_MINI_LIST_ITEM                   1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS    0
#define configUSE_TIMERS                           1
#define configTIMER_TASK_PRIORITY                   ( configMAX_PRIORITIES - 1 )
#define configTIMER_TASK_STACK_DEPTH                configMINIMAL_STACK_SIZE
#define configTIMER_QUEUE_LENGTH                   10
#define configUSE_EVENT_GROUPS                     1
#define configUSE_STREAM_BUFFERS                   1
#define configSUPPORT_STATIC_ALLOCATION             1
#define configSUPPORT_DYNAMIC_ALLOCATION            1
#define configTOTAL_HEAP_SIZE                       ( ( size_t ) 8192 )
#define configAPPLICATION_ALLOCATED_HEAP             0
#define configKERNEL_INTERRUPT_PRIORITY            255
/* Lowest interrupt priority that can call FreeRTOS API (Cortex-M: 5–15 typical). */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY       5
#define configMAX_API_CALL_INTERRUPT_PRIORITY      configMAX_SYSCALL_INTERRUPT_PRIORITY
#define configUSE_IDLE_HOOK                        0
#define configUSE_TICK_HOOK                        0
#define configUSE_MALLOC_FAILED_HOOK                0
#define configCHECK_FOR_STACK_OVERFLOW              2
#define configUSE_TRACE_FACILITY                   0
#define configUSE_STATS_FORMATTING_FUNCTIONS        0
#define configUSE_CO_ROUTINES                      0
#define configUSE_16_BIT_TICKS                     0

#define configASSERT( x ) \
  do { if ( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for ( ;; ) ; } } while ( 0 )

#endif /* FREERTOS_CONFIG_H */
