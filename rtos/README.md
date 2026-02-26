# FreeRTOS in STM32 Suite (Riviera Tech)

This folder contains **FreeRTOS kernel and GCC ARM Cortex-M ports** so you can download a ready-to-build STM32 + FreeRTOS project from our Open Source Libraries.

- **FreeRTOS** (C) Real Time Engineers Ltd.  
  https://www.FreeRTOS.org  
  https://github.com/FreeRTOS/FreeRTOS-Kernel  
  License: MIT (see `freertos/LICENSE.md` and headers in source files).

When you select **FreeRTOS (kernel + port)** in the folder tree and download the zip, you get:

- Kernel sources (tasks, queue, list, timers, event_groups, stream_buffer)
- The **correct port** for your selected MCU (ARM_CM0, ARM_CM3, ARM_CM4F, ARM_CM7/r0p1, or ARM_CM33_NTZ)
- `FreeRTOSConfig.h` (in `config/`) – set `configCPU_CLOCK_HZ` to your system clock
- heap_4.c (portable/MemMang)
- `examples/health_supervisor.c/.h` – watchdog-friendly heartbeat and recovery supervision template

Add the `rtos/freertos` path to your include and source paths, then call `vTaskStartScheduler()` from your `main()` after creating your tasks.
