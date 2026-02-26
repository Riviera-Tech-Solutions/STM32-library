/**
 * Minimal FreeRTOS example for STM32 (Riviera Tech STM32 Suite)
 * Copy the task creation and vTaskStartScheduler() pattern into your main.c.
 *
 * Your project must include: FreeRTOS kernel .c, portable/GCC/<your port>, portable/MemMang/heap_4.c,
 * and FreeRTOSConfig.h (set configCPU_CLOCK_HZ to your system clock).
 * Optional: add freertos_hooks.c from this folder and set configCHECK_FOR_STACK_OVERFLOW 2 and
 * configUSE_MALLOC_FAILED_HOOK 1 in FreeRTOSConfig.h for easier debugging of overflows and heap failures.
 * Optional for long-run robustness: include health_supervisor.c/.h and register task heartbeats + recover callbacks.
 */

#include "FreeRTOS.h"
#include "task.h"

static void vBlinkTask(void *pvParameters);
static void vHeartbeatTask(void *pvParameters);

int main(void)
{
    /* System and clock init (your STM32 init code here). */

    xTaskCreate(vBlinkTask, "Blink", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vHeartbeatTask, "Heartbeat", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    vTaskStartScheduler();
    for (;;)
        ;
}

static void vBlinkTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        /* Toggle LED or GPIO here. */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void vHeartbeatTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
