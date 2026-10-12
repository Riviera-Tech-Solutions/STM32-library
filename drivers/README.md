# STM32 polled drivers

Define exactly one family in compiler flags. Family mappings do not guarantee that every part/package contains every peripheral instance. Set board masks and configure pins before use. Automated checks do not certify physical hardware.

| Family | GPIO A-G | I2C/SPI | UART/ADC/PWM CH1 | IWDG |
|---|---|---|---|---|
| F0, F1, F4, F7, G0, L4, L4PLUS | Mapped | Mapped | Mapped | Mapped |
| F2, F3, G4, L0, L1 | Mapped | Mapped | Unsupported | Mapped |
| H7 | Mapped | Unsupported | Unsupported | IWDG1 |
| H5, L5, U5, WB, WL | Mapped | Unsupported | Unsupported | Unsupported |

Prefix family names with STM32, e.g. -DSTM32G0. Unsupported initialization returns false (watchdog: zero). Fault handlers build for M0/M0+/M3/M4/M7/M33. H5/L5/U5 GPIO uses nonsecure addresses and requires correct security attribution.

## API contracts

- GPIO: GPIO_PORT_MASK defaults to A-G. Writes use atomic BSRR. Serialize configuration calls. gpio_set_af selects AF0-15 on MODER families; F1 returns false and needs board-specific AFIO remapping. gpio_set_open_drain configures output type. On F1 set output/AF mode before output type, and input mode before pull. No-pull means floating; analog mode is preserved.
- SPI: instances 0/1 are SPI1/SPI2; SPI_INSTANCE_MASK defaults to 3. Mode 0, 8-bit, MSB first, software NSS high. Caller configures AF pins and GPIO CS. clock_hz is a maximum; requests below PCLK/256 are rejected. On failure release CS, reinitialize and start a new transaction. No automatic replay.
- I2C: instances 0/1 are I2C1/I2C2; I2C_INSTANCE_MASK defaults to 3. Caller supplies AF/open-drain pins and external pull-ups. Serialized single-master transactions; no combined repeated-START API. Speeds 10-400 kHz; zero selects 100 kHz. V2 transactions above 255 bytes are rejected before MMIO. Split only if the slave permits STOP between chunks. I2C_MAX_RETRIES is 0-10, default 2; retries can repeat accepted bytes, so use 0 for non-idempotent commands. Recovery handles peripheral state, not electrically stuck slaves.
- UART: instances 0/1 are USART1/USART2; UART_INSTANCE_MASK defaults to 3. 8-N-1, oversampling by 16. Rounded BRR is validated; UART_MAX_BAUD_ERROR_PPM defaults to 20000 (2%). uart_get_stats reports TX failure. TXE completion does not mean the final stop bit has left the wire.
- PWM: instances 0/1 are TIM2/TIM3; TIM_INSTANCE_MASK defaults to 3. Only channel 1 is supported; other channels and zero/unrepresentable frequencies are rejected. An update event loads the prescaler before counting. ARR is limited so 100% duty is representable on 16-bit timers. No input capture.
- ADC: owns ADC1 and its common prescaler; do not mix clock ownership. Configure analog pins. ADC_CHANNEL_MASK defaults to external inputs (0-15; L4 1-16); narrow for the part. Internal channels require application-specific enable/settling. Initialization stops/disables before calibration, uses conservative divisors and long sample times. adc_read zero can mean 0 V or failure: inspect last_error. Reinitialize after timeout.
- IWDG: starts before synchronizing configuration, enabling LSI. Zero signals unsupported hardware or timeout; failure cannot stop an already-started watchdog. WDG_LSI_HZ defaults to nominal 40 kHz on F0/F1/F3 and 32 kHz otherwise; override for the actual oscillator. Reset-cause reads clear all RCC reset flags.

## Clock and timing settings

Startup is for cold reset. F4 configures HCLK/APB2=84 MHz, APB1=42 MHz. Other SystemInit templates preserve reset clocks: F0/F1/F3 8 MHz; F2/F7/G0/G4 16 MHz; L0/L1 about 2.097 MHz; L4/L4PLUS 4 MHz. Bootloader handoff and changed clocks require an exact-device SystemInit and matching SPI_PCLK1_HZ, SPI_PCLK2_HZ, UART_PCLK1_HZ, UART_PCLK2_HZ, I2C_KERNEL_HZ, TIM_CLK_HZ and ADC_INPUT_CLOCK_HZ.

ADC divides APB2 by 6 on F1, by 4 on F4/F7, PCLK by 4 on F0/G0 and HCLK by 4 on L4. Check supply-dependent device limits. ADC_VREG_SETTLE_LOOPS defaults to 4000; validate regulator/startup delays on hardware. I2C v2 timing is approximate; supply both I2C_V2_TIMINGR_100K and I2C_V2_TIMINGR_400K for board-validated values.

Without a tick, timeouts are finite spin budgets, not calibrated milliseconds. STM32_WAIT_USE_TICK=1 requires STM32_WAIT_GET_TICK_MS() with a visible declaration, e.g. via a forced-include board header. Missing hooks fail compilation. Unsigned subtraction handles wrap and the spin fallback remains active when ticks stop.

| Driver | Millisecond limit | Spin limit |
|---|---|---|
| SPI | SPI_TIMEOUT_MS=50 | SPI_TIMEOUT_LOOPS=1000000 |
| UART | UART_TX_TIMEOUT_MS=100 | UART_TX_TIMEOUT_LOOPS=1000000 |
| ADC | ADC_TIMEOUT_MS=100 | ADC_TIMEOUT_LOOPS=1000000 |
| IWDG | WDG_TIMEOUT_MS=1000 | WDG_TIMEOUT_LOOPS=1000000 |
| I2C | I2C_TIMEOUT_MS=100 | milliseconds x 1000 |

Limits are per wait, not whole-operation deadlines. Serialize driver calls; telemetry is not atomic between tasks/interrupts.

## Verification and board integration

From code/, run node tests/run.cjs with host GCC and node tests/build.cjs with arm-none-eabi-gcc. Override CC, ARM_CC and ARM_OBJDUMP as needed. Register-model tests instrument MMIO expressions; they do not emulate silicon. ARM compilation/linking uses unchanged production source.

Startup copies data, clears BSS and retains .noinit outside the heap. External IRQ slots are placeholders: replace with the exact device's CMSIS vector table before enabling peripheral interrupts. Set linker memory origins/lengths to the part. M33 security, multicore boot and bootloader handoff require board integration. See [hardware checks](../tests/HARDWARE.md).
