# Drivers

- **gpio/** – General-purpose I/O (all families)
- **i2c/** – I2C master (all families)
- **spi/** – SPI master (all families)
- **uart/** – UART / USART (F4, G0; others stub)
- **tim/** – Timer PWM (F4, G0; others stub)
- **adc/** – ADC single-channel (F4, G0; others stub)

One .c + one .h per driver. Config via defines in header.

## Define your MCU family

**In your build (compiler options), define exactly one of:**

`STM32F0` `STM32F1` `STM32F2` `STM32F3` `STM32F4` `STM32F7` `STM32G0` `STM32G4` `STM32H5` `STM32H7` `STM32L0` `STM32L1` `STM32L4` `STM32L4PLUS` `STM32L5` `STM32U5` `STM32WB` `STM32WL`

Example: `-DSTM32F4` or `-DSTM32G0`. This selects the correct register code for GPIO, I2C, SPI (and UART/TIM/ADC where implemented).

## Robustness defaults

- `I2C_TIMEOUT_MS` (default `100`) controls I2C blocking timeout guard.
- `I2C_MAX_RETRIES` (default `2`) retries failed I2C transactions with recovery.
- `ADC_TIMEOUT_LOOPS` (default `1000000`) bounds ADC poll loops.
- `UART_TX_TIMEOUT_LOOPS` (default `1000000`) bounds UART TXE polling.

Define these in your build flags as needed for your CPU speed and fault-handling policy.

## Timeout pattern configuration

- `STM32_WAIT_USE_TICK=1` enables timeout checks using unsigned tick math.
- `STM32_WAIT_GET_TICK_MS()` should return system uptime in milliseconds (e.g. your `millis()`).
- If tick mode is off (`0`), waits still remain bounded via spin limits.

## Runtime health telemetry

- I2C exposes `i2c_get_stats()` / `i2c_reset_stats()` and tracks retries/recovery/failures.
- ADC exposes `adc_get_stats()` / `adc_reset_stats()` and tracks read failures/timeouts.
- UART exposes `uart_get_stats()` / `uart_reset_stats()` and tracks TX timeouts/RX empties.

Use these counters in production logs/watchdog logic to detect degraded wiring, flaky sensors, or bus instability before full outages.
