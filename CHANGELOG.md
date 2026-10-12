# 2026-09-27 driver correctness update

- SPI: byte-width FIFO access, unconditional RX draining, speed selection, software NSS, validation and timeout/error propagation.
- I2C: length-specific legacy receive termination, protected register transitions, error polling and final-attempt cleanup; rejects v2 transfers above 255 bytes.
- Shared waits: explicit tick hook, wrap-safe timeouts and finite spin fallback. UART/ADC millisecond limits are separate from loop counts.
- IWDG: start before synchronization, bounded waits, saturated prescaler and nominal family LSI rates; unverified families fail explicitly.
- PWM: channel/frequency validation, prescaler update before first period, representable 100% duty.
- GPIO: atomic output, correct F1 no-pull, analog preservation, AF/open-drain helpers and board masks.
- UART: rounded divider, range and baud-error validation.
- ADC: safe divisors, stop/disable before recalibration, stabilization/sample times, channel validation, G0 channel-ready wait, failed-read invalidation.
- Fault/startup: volatile retained records, GNU startup templates, explicit NOLOAD region outside BSS/heap. Non-F4 SystemInit preserves cold-reset clocks instead of guessing RCC layouts.
- Regression, ARM build/link and generated-download coverage. Physical hardware remains unqualified.

Compatibility: unsupported initialization, oversized v2 I2C transfers, unsupported PWM channels, impossible SPI/baud settings and missing/ambiguous family defines are rejected. Review the [driver guide](drivers/README.md) before updating a board project.
# Buildable downloads and AI-tool integration

- One CMake/Ninja build for bare-metal and FreeRTOS ZIPs, including a heartbeat application, selected drivers and transitive peripheral dependencies.
- Fixed FreeRTOS tick-width conflict, clock defaults, interrupt-priority encoding and scheduler handler routing; no static-allocation callbacks are required by the starter.
- Exact-part memory profiles for F401CC/CE/RE, G071RB and F103C8. Other selections explicitly remain unverified templates; flash requires board and memory confirmation.
- Plain-data configuration and consistent PowerShell/POSIX runners; build works offline without confirmed hardware. No silent tool installation or guessed flash commands.
- Public generator and CLI mirror website sources, with actual ZIP compilation and mocked flash-command regression tests.
- F4 84 MHz profile uses a 336 MHz PLL VCO and rejects unsupported clock overrides.

