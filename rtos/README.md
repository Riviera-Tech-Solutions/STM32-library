# FreeRTOS in STM32 Suite (Riviera Tech Solutions)

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
- A generated root `main.c` heartbeat task and CMake build. The source repository also provides optional health-supervisor examples.

Use the website download or `create-project.cjs --freertos` for a core-specific configuration and build files. The source-tree config is an F4 example, not a universal config. Generated configs use the shipped reset/clock setup, scaled interrupt priorities and direct SVC/PendSV/SysTick aliases. Check clock rate and interrupt priority bits against the actual device before using peripheral interrupts with RTOS APIs.

Build: `cmake -S . -B build -G Ninja` then `cmake --build build`. The heartbeat task is already created in `main.c`. Board testing is still required.
