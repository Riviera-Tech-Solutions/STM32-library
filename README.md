# STM32 code suite – embedded C

- **core/** – Startup, system, CMSIS per Cortex type (M0, M3, M4, M7, M33).
- **drivers/** – GPIO, I2C, SPI, UART, TIM, ADC (one folder per driver).
- **peripherals/** – LCD 16x2 and other external modules.

Each driver/peripheral is optimized C with clear APIs and no magic numbers. Family-specific code uses `#if defined(STM32F4)` etc. where needed.
