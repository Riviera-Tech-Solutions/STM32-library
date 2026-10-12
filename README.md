# STM32 embedded C suite — Riviera Tech Solutions

Get a project from the [official library downloads](https://rivieratech.solutions/open-source?tab=libraries). Select an MCU and modules, extract the ZIP, and open the folder in your preferred editor or AI coding tool. `AGENTS.md` describes the project, build commands and board checks.

The download includes `main.c`, CMake, GNU startup, a linker script, selected drivers and optional FreeRTOS. The starter increments a `heartbeat` counter visible in a debugger; it deliberately does not assume an LED pin.

Install Arm GNU Toolchain (`arm-none-eabi-gcc` with newlib), CMake 3.16+ and Ninja, and add their executable directories to PATH. Node is **not** needed in a downloaded project.

```powershell
.\rts.cmd doctor
.\rts.cmd build
```

On Linux/macOS use `sh rts.sh doctor` and `sh rts.sh build`. Both work without a probe. Output is `build/firmware.elf`, `.bin` and `.hex`. `doctor` checks tools only; it does not install software or validate hardware.

Before `flash`, read `PRE_FLASH_CHECKLIST.txt` and confirm your exact board and memory in `rts.env`. Choose STM32CubeProgrammer or an OpenOCD board configuration. Never assume the MCU family determines SRAM size, pin routing, supply or clock configuration. Flashing replaces the target application; build success is not board qualification.

Exact memory profiles currently cover F401CC/CE/RE, G071RB and F103C8. Other catalog selections produce explicitly unverified family templates that require manual memory configuration before flashing. Startup targets cold reset at `0x08000000`; peripheral IRQ tables, M33 security, multicore boot and bootloaders need device-specific integration. F4 defaults to 84 MHz at 2.7–3.6 V; other families retain reset clocks.

## Generate the same project locally

In this source repository, install Node 20+ and run:

```sh
npm ci
node create-project.cjs STM32F401RE project.zip --freertos
```

Omit `--freertos` for bare metal. The CLI includes drivers; use the website to choose individual modules and peripherals. The generator and data are published alongside the code so website downloads and GitHub share the same implementation.

core/ contains cold-reset startup/system/linker templates; drivers/ contains register-level polled drivers; peripherals/ contains external-device helpers; rtos/ provides optional FreeRTOS sources.

Start with the [driver guide](drivers/README.md), [update notes](CHANGELOG.md) and [board checks](tests/HARDWARE.md). Select exact-part memory, pins, clocks and available peripheral instances before flashing. IRQ tables and security startup require board integration.

Run node tests/run.cjs for host regressions and node tests/build.cjs for ARM compile/link checks. Automated checks do not replace hardware validation.

`node tests/projects.cjs` builds actual extracted ZIPs, including FreeRTOS, and checks that unconfirmed flashing is blocked. `FULL_MATRIX=1` covers all family/core combinations. These tests need the build tools above. See [board checks](tests/HARDWARE.md) for physical validation still required.
