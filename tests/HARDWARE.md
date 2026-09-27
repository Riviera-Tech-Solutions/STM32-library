# Board validation

These tests require the actual board and are not recorded as passed by automated build/model checks. Use an exact-part legacy target (e.g. F401/F103) and a v2 target (e.g. G071), with verified pins, clocks, memory and instance masks.

1. SPI loopback and transmit-only traffic: one/odd/many bytes, SCK limits, CS/framing, overrun/busy failure and reinitialization.
2. I2C reads of 1, 2, 3, 4 and 6 bytes: inspect final NACK/STOP. Test v2 255 bytes and 256-byte rejection, missing slave, NACK, unplug/replug and retry cleanup.
3. PWM: scope first period, reinitialization and 0/50/100% duty.
4. ADC: grounded/reference inputs, multiple channels, repeated init, timeout/reinit; verify clock, regulator delay and acquisition time.
5. UART: loopback and measured baud error at actual APB clocks.
6. GPIO: F1 no-pull/up/down, open-drain release, AF assignment, independent concurrent output writes.
7. Watchdog: cold boot with LSI off, requested/max timeout, kicks, missed kick and reset-cause reporting.
8. Controlled fault: reset, read retained record, clear it, reset again; verify heap/stack separation and actual RTOS stack behavior.

Record part, board, clock tree, compiler/options and waveforms. M33 security transitions, exact IRQ routing, multicore boot and bootloader handoff need separate integration checks.
