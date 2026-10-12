/*
 * FreeRTOS Kernel - configuration for STM32 (Riviera Tech Solutions STM32 Suite)
 * Auto-generated at zip build time for MCU: STM32F401RE (F4, cortex-m4).
 * Quick start: verify configCPU_CLOCK_HZ below matches your exact SYSCLK in Hz.
 *
 * FreeRTOS Kernel (C) Real Time Engineers Ltd. https://www.FreeRTOS.org
 * SPDX-License-Identifier: MIT
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* Required by newer FreeRTOS GCC ports (CM0/CM33 and others). */
#define configENABLE_MPU                         0
#define configENABLE_FPU                         1
#define configENABLE_TRUSTZONE                   0

/* Typical default for selected MCU family/core; verify against your board clock tree. */
#define configCPU_CLOCK_HZ    ( ( unsigned long ) 84000000 )

#define configTICK_RATE_HZ                         ( ( TickType_t ) 1000 )
#define configUSE_PREEMPTION                        1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    1
#define configUSE_TICKLESS_IDLE                     0
#define configMAX_PRIORITIES                        8
#define configMINIMAL_STACK_SIZE                    ( ( uint16_t ) 128 )
#define configMAX_TASK_NAME_LEN                     16
#define configTICK_TYPE_WIDTH_IN_BITS              TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                     1
#define configUSE_TASK_NOTIFICATIONS                1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES       1
#define configQUEUE_REGISTRY_SIZE                   0
#define configUSE_MINI_LIST_ITEM                    1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS     0
#define configUSE_TIMERS                            1
#define configTIMER_TASK_PRIORITY                   ( configMAX_PRIORITIES - 1 )
#define configTIMER_TASK_STACK_DEPTH                configMINIMAL_STACK_SIZE
#define configTIMER_QUEUE_LENGTH                    10
#define configUSE_EVENT_GROUPS                      1
#define configUSE_STREAM_BUFFERS                    1
#define configSUPPORT_STATIC_ALLOCATION             0
#define configSUPPORT_DYNAMIC_ALLOCATION            1
#define configTOTAL_HEAP_SIZE                       ( ( size_t ) 4096 )
#define configAPPLICATION_ALLOCATED_HEAP            0
#define configKERNEL_INTERRUPT_PRIORITY             255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY        (5U << 4)
#define configMAX_API_CALL_INTERRUPT_PRIORITY       configMAX_SYSCALL_INTERRUPT_PRIORITY
#define configUSE_IDLE_HOOK                         0
#define configUSE_TICK_HOOK                         0
#define configUSE_MALLOC_FAILED_HOOK                0
#define configCHECK_FOR_STACK_OVERFLOW              2
#define configUSE_TRACE_FACILITY                    0
#define configUSE_STATS_FORMATTING_FUNCTIONS        0
#define configUSE_CO_ROUTINES                       0
#define INCLUDE_vTaskDelay                         1
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
/* M0 lacks VTOR; other cores can validate direct vector routing. */
#define configCHECK_HANDLER_INSTALLATION 1

#define configASSERT( x ) \
  do { if ( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for ( ;; ) ; } } while ( 0 )

#endif /* FREERTOS_CONFIG_H */
