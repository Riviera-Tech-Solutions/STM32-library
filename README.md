# STM32 embedded C suite

core/ contains cold-reset startup/system/linker templates; drivers/ contains register-level polled drivers; peripherals/ contains external-device helpers; rtos/ provides optional FreeRTOS sources.

Start with the [driver guide](drivers/README.md), [update notes](CHANGELOG.md) and [board checks](tests/HARDWARE.md). Select exact-part memory, pins, clocks and available peripheral instances before flashing. IRQ tables and security startup require board integration.

Run node tests/run.cjs for host regressions and node tests/build.cjs for ARM compile/link checks. Automated checks do not replace hardware validation.
