/**
 * STM32 code suite – zip builder service
 * Resolves selected nodeIds + MCU core to file paths, collects files from code/, streams zip.
 * No path traversal: only node ids from tree and mcu from list are allowed.
 */

const path = require('path');
const fs = require('fs');
const archiver = require('archiver');
function defaultCodeDir() {
  const nested = path.join(__dirname, '../code');
  return fs.existsSync(path.join(nested, 'core')) ? nested : path.join(__dirname, '..');
}
function exactMemory(name) {
  const profiles = require('../data/memoryProfiles.json');
  return profiles[String(name || '').toUpperCase()] || null;
}
function buildMainContent(rtos) {
  if (!rtos) return '#include <stdint.h>\nvolatile uint32_t heartbeat;\nint main(void) { for (;;) { ++heartbeat; __asm volatile ("nop"); } }\n';
  return '#include "FreeRTOS.h"\n#include "task.h"\nvolatile uint32_t heartbeat;\nstatic void heartbeat_task(void *arg) { (void)arg; for (;;) { ++heartbeat; vTaskDelay(pdMS_TO_TICKS(500)); } }\nvoid vApplicationStackOverflowHook(TaskHandle_t task, char *name) { (void)task; (void)name; taskDISABLE_INTERRUPTS(); for (;;) {} }\nint main(void) { if (xTaskCreate(heartbeat_task, "heartbeat", 256, NULL, 1, NULL) != pdPASS) { for (;;) {} } vTaskStartScheduler(); for (;;) {} }\n';
}

/**
 * Timestamped log for instrumentation.
 */
function log(label, data = null) {
  const ts = new Date().toISOString();
  if (data !== null && typeof data === 'object') {
    console.log(`[${ts}] [zipBuilder:${label}]`, JSON.stringify(data));
  } else {
    console.log(`[${ts}] [zipBuilder:${label}]`, data !== null ? data : '');
  }
}

/**
 * Collect all node ids that should be included: core (and its descendants) always;
 * for nodeIds, expand each to itself and all descendants that have a path.
 * @param {object} nodes - tree nodes map
 * @param {string} coreNodeId - e.g. 'core'
 * @param {string[]} nodeIds - selected node ids from client
 * @returns {Set<string>} set of node ids to include
 */
function collectNodeIdsToInclude(nodes, coreNodeId, nodeIds) {
  const result = new Set();
  const core = nodes[coreNodeId];
  if (core) {
    result.add(core.id);
    const collectChildren = (node) => {
      (node.children || []).forEach((cid) => {
        const child = nodes[cid];
        if (child) {
          result.add(child.id);
          collectChildren(child);
        }
      });
    };
    collectChildren(core);
  }
  nodeIds.forEach((id) => {
    const node = nodes[id];
    if (!node) return;
    result.add(id);
    const collectChildren = (n) => {
      (n.children || []).forEach((cid) => {
        const child = nodes[cid];
        if (child) {
          result.add(child.id);
          collectChildren(child);
        }
      });
    };
    collectChildren(node);
  });
  const visit = id => {
    const node = nodes[id];
    for (const dep of node?.dependsOn || []) {
      if (!result.has(dep) && nodes[dep]) { result.add(dep); visit(dep); }
    }
  };
  for (const id of result) visit(id);
  return result;
}

/**
 * Map MCU family to core subfolder name (e.g. F4 -> stm32f4, L4+ -> stm32l4plus).
 */
function familyToCoreSubfolder(family) {
  if (!family || typeof family !== 'string') return '';
  const f = family.toLowerCase().replace('+', 'plus');
  return `stm32${f}`;
}

/**
 * Map MCU core to FreeRTOS GCC port folder (relative to portable/GCC/).
 * Used when rtos_freertos is selected so we include only the correct port.
 */
function getFreeRTOSPortDir(mcuCore) {
  const core = (mcuCore || '').toLowerCase();
  if (core === 'cortex-m0' || core === 'cortex-m0plus') return 'ARM_CM0';
  if (core === 'cortex-m3') return 'ARM_CM3';
  if (core === 'cortex-m4') return 'ARM_CM4F';
  if (core === 'cortex-m7') return 'ARM_CM7/r0p1';
  if (core === 'cortex-m33') return 'ARM_CM33_NTZ/non_secure';
  return 'ARM_CM4F';
}

/**
 * Return a practical default SYSCLK value by STM32 family.
 * This is a starter value to reduce setup friction; users should still verify against board clock config.
 */
function getDefaultSysClockHz(family) {
  // Match the shipped SystemInit, not the maximum advertised CPU frequency.
  return ({F0:8000000,F1:8000000,F2:16000000,F3:8000000,F4:84000000,
    F7:16000000,G0:16000000,G4:16000000,L0:2097152,L1:2097152,
    L4:4000000,L4PLUS:4000000,'L4+':4000000,H7:64000000,H5:64000000,L5:4000000,
    U5:4000000,WB:4000000,WL:4000000})[String(family).toUpperCase()] || 16000000;
}

/**
 * Build a core-aware FreeRTOSConfig.h so selected FreeRTOS ports compile out-of-the-box.
 * Users can still edit clock, heap, priorities, and optional security knobs later.
 */
function buildFreeRTOSConfigContent(mcuCore, mcuFamily = '', mcuName = '') {
  const core = (mcuCore || '').toLowerCase();
  const family = (mcuFamily || '').toUpperCase();
  const useOptimizedTaskSelect = (core === 'cortex-m0' || core === 'cortex-m0plus') ? 0 : 1;
  const enableMpu = 0;
  const enableTrustZone = 0;
  const enableFpu = (core === 'cortex-m4' || core === 'cortex-m7') ? 1 : 0;
  const cpuClockHzDefault = getDefaultSysClockHz(family, core);

  return [
    `/*`,
    ` * FreeRTOS Kernel - configuration for STM32 (Riviera Tech Solutions STM32 Suite)`,
    ` * Auto-generated at zip build time for MCU: ${mcuName || 'unknown'} (${family || 'unknown family'}, ${core || 'unknown core'}).`,
    ` * Quick start: verify configCPU_CLOCK_HZ below matches your exact SYSCLK in Hz.`,
    ` *`,
    ` * FreeRTOS Kernel (C) Real Time Engineers Ltd. https://www.FreeRTOS.org`,
    ` * SPDX-License-Identifier: MIT`,
    ` */`,
    ``,
    `#ifndef FREERTOS_CONFIG_H`,
    `#define FREERTOS_CONFIG_H`,
    ``,
    `#include <stdint.h>`,
    ``,
    `/* Required by newer FreeRTOS GCC ports (CM0/CM33 and others). */`,
    `#define configENABLE_MPU                         ${enableMpu}`,
    `#define configENABLE_FPU                         ${enableFpu}`,
    `#define configENABLE_TRUSTZONE                   ${enableTrustZone}`,
    ``,
    `/* Typical default for selected MCU family/core; verify against your board clock tree. */`,
    `#define configCPU_CLOCK_HZ    ( ( unsigned long ) ${cpuClockHzDefault} )`,
    ``,
    `#define configTICK_RATE_HZ                         ( ( TickType_t ) 1000 )`,
    `#define configUSE_PREEMPTION                        1`,
    `#define configUSE_PORT_OPTIMISED_TASK_SELECTION    ${useOptimizedTaskSelect}`,
    `#define configUSE_TICKLESS_IDLE                     0`,
    `#define configMAX_PRIORITIES                        8`,
    `#define configMINIMAL_STACK_SIZE                    ( ( uint16_t ) 128 )`,
    `#define configMAX_TASK_NAME_LEN                     16`,
    `#define configTICK_TYPE_WIDTH_IN_BITS              TICK_TYPE_WIDTH_32_BITS`,
    `#define configIDLE_SHOULD_YIELD                     1`,
    `#define configUSE_TASK_NOTIFICATIONS                1`,
    `#define configTASK_NOTIFICATION_ARRAY_ENTRIES       1`,
    `#define configQUEUE_REGISTRY_SIZE                   0`,
    `#define configUSE_MINI_LIST_ITEM                    1`,
    `#define configNUM_THREAD_LOCAL_STORAGE_POINTERS     0`,
    `#define configUSE_TIMERS                            1`,
    `#define configTIMER_TASK_PRIORITY                   ( configMAX_PRIORITIES - 1 )`,
    `#define configTIMER_TASK_STACK_DEPTH                configMINIMAL_STACK_SIZE`,
    `#define configTIMER_QUEUE_LENGTH                    10`,
    `#define configUSE_EVENT_GROUPS                      1`,
    `#define configUSE_STREAM_BUFFERS                    1`,
    `#define configSUPPORT_STATIC_ALLOCATION             0`,
    `#define configSUPPORT_DYNAMIC_ALLOCATION            1`,
    `#define configTOTAL_HEAP_SIZE                       ( ( size_t ) 4096 )`,
    `#define configAPPLICATION_ALLOCATED_HEAP            0`,
    `#define configKERNEL_INTERRUPT_PRIORITY             255`,
    `#define configMAX_SYSCALL_INTERRUPT_PRIORITY        (5U << 4)`,
    `#define configMAX_API_CALL_INTERRUPT_PRIORITY       configMAX_SYSCALL_INTERRUPT_PRIORITY`,
    `#define configUSE_IDLE_HOOK                         0`,
    `#define configUSE_TICK_HOOK                         0`,
    `#define configUSE_MALLOC_FAILED_HOOK                0`,
    `#define configCHECK_FOR_STACK_OVERFLOW              2`,
    `#define configUSE_TRACE_FACILITY                    0`,
    `#define configUSE_STATS_FORMATTING_FUNCTIONS        0`,
    `#define configUSE_CO_ROUTINES                       0`,
    `#define INCLUDE_vTaskDelay                         1`,
    `#define vPortSVCHandler SVC_Handler`,
    `#define xPortPendSVHandler PendSV_Handler`,
    `#define xPortSysTickHandler SysTick_Handler`,
    `/* M0 lacks VTOR; other cores can validate direct vector routing. */`,
    `#define configCHECK_HANDLER_INSTALLATION ${core === 'cortex-m0' ? 0 : 1}`,
    ``,
    `#define configASSERT( x ) \\`,
    `  do { if ( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for ( ;; ) ; } } while ( 0 )`,
    ``,
    `#endif /* FREERTOS_CONFIG_H */`,
    ``,
  ].join('\n');
}

function buildRtsConfigContent(mcu) {
  return JSON.stringify({project:'Riviera Tech Solutions STM32 starter', mcu,
    memoryProfile: exactMemory(mcu?.name || mcu?.id), config:'rts.env',
    build:['cmake','--build','build'], artifact:'build/firmware.bin',
    hardwareValidated:false}, null, 2)+'\n';
}

function buildRtsEnvContent() {
  return ['# Plain KEY=value data; do not add shell quoting or commands.',
    'BOARD_LABEL=SET_THIS_TO_YOUR_EXACT_BOARD','BOARD_CONFIG_CONFIRMED=false',
    'MEMORY_CONFIG_CONFIRMED=false','TOOL=stm32_programmer_cli','INTERFACE=SWD',
    'FLASH_ADDRESS=0x08000000','OPENOCD_CFG=','PROBE_SERIAL=',''].join('\n');
}

function buildPreFlashChecklistContent() {
  return ['Before flash: confirm the exact board, chip marking, power and SWD wiring.',
    'Check core/linker.ld memory against the chip datasheet, including SRAM bank origins.',
    'Check SystemInit clock settings and driver clocks. No peripheral pins are selected by default.',
    'Confirm the connected probe (PROBE_SERIAL for CubeProgrammer; board config for OpenOCD).',
    'Set BOARD_LABEL, BOARD_CONFIG_CONFIRMED=true and MEMORY_CONFIG_CONFIRMED=true in rts.env.',
    'M33 security, multicore targets, bootloader offsets and peripheral IRQ tables need device-specific integration.',
    'Flashing overwrites application firmware. Run only on the intended target with user authorization.',
    'Then use .\\rts.cmd flash on Windows, or sh rts.sh flash on Linux/macOS.',''].join('\n');
}

function buildAgentFlashGuideContent(mcu) {
  return ['# STM32 project instructions for AI tools', '',
    'Selected MCU: '+(mcu?.name || mcu?.id)+'. Read README.txt, drivers/README.md and rts.json first.',
    'Edit main.c for the application. A heartbeat counter is provided; no LED pin is guessed.',
    'Build immediately without hardware: Windows .\\rts.cmd build; Linux/macOS sh rts.sh build.',
    'Dependencies: Arm GNU arm-none-eabi-gcc (with newlib), CMake 3.16+, Ninja.',
    'doctor checks tools only. It does not install software or verify a connected chip.',
    'Never claim hardware validation from a successful build.',
    'Before flashing, follow PRE_FLASH_CHECKLIST.txt. Do not set confirmation flags on behalf of unknown hardware.',
    'rts.env is plain data, not executable shell. It is the authoritative flash configuration.',
    'Unknown memory profiles are explicitly templates. Verify and edit core/linker.ld before confirming memory.',
    'Use GPIO/AF/clock configuration documented by the board vendor. Unsupported driver initialization can fail.',
    'Commands: Windows .\\rts.cmd doctor/build/flash; Linux/macOS sh rts.sh doctor/build/flash.',
    'No automatic erase-all, option-byte changes, protection removal, or unlimited retries.',
    'On failure preserve the tool output and investigate code, configuration, wiring and power.',''].join('\n');
}

function buildRtsPowerShellContent() {
  return fs.readFileSync(path.join(__dirname,'templates/rts.ps1'),'utf8');
}
function buildRtsBashContent() {
  return fs.readFileSync(path.join(__dirname,'templates/rts.sh'),'utf8');
}

function buildRtsCmdWrapperContent() {
  return [
    '@echo off',
    'powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0\\rts.ps1" %*',
    '',
  ].join('\r\n');
}

/**
 * Get list of { zipPathPrefix, sourceDir } for each included node that has a path.
 * Core: try codeDir/core/<mcuCore>/<mcuFamily> first (e.g. cortex-m4/stm32f4), then fallback to codeDir/core/<mcuCore>.
 * Others: sourceDir = codeDir/node.path, zipPathPrefix = node.path.
 */
function getPathsToAdd(nodes, includedIds, mcuCore, codeDir, mcuFamily = '') {
  const list = [];
  const coreNode = nodes.core;
  const familyFolder = familyToCoreSubfolder(mcuFamily);
  let coreDir = path.join(codeDir, 'core', mcuCore, familyFolder);
  if (!fs.existsSync(coreDir)) {
    coreDir = path.join(codeDir, 'core', mcuCore);
  }
  if (coreNode && includedIds.has(coreNode.id) && fs.existsSync(coreDir)) {
    list.push({ zipPathPrefix: 'core', sourceDir: coreDir });
  }
  includedIds.forEach((id) => {
    if (id === 'rtos_freertos' || id === 'rtos') return;
    const node = nodes[id];
    if (!node || !node.path || node.id === 'core') return;
    const sourceDir = path.join(codeDir, node.path);
    if (fs.existsSync(sourceDir)) {
      list.push({ zipPathPrefix: node.path, sourceDir });
    }
  });
  /* FreeRTOS: add kernel (root .c + include + config), MemMang, and only the port for this MCU core */
  if (includedIds.has('rtos_freertos')) {
    const freertosBase = path.join(codeDir, 'rtos', 'freertos');
    if (fs.existsSync(freertosBase)) {
      list.push({ zipPathPrefix: 'rtos/freertos', sourceDir: freertosBase, freertosKernelOnly: true });
      const memMangDir = path.join(freertosBase, 'portable', 'MemMang');
      if (fs.existsSync(memMangDir)) {
        list.push({ zipPathPrefix: 'rtos/freertos/portable/MemMang', sourceDir: memMangDir });
      }
      const portDirRel = getFreeRTOSPortDir(mcuCore);
      const portDirAbs = path.join(freertosBase, 'portable', 'GCC', ...portDirRel.split('/'));
      if (fs.existsSync(portDirAbs)) {
        list.push({ zipPathPrefix: `rtos/freertos/portable/GCC/${portDirRel}`, sourceDir: portDirAbs });
      }
      log('freertos:port', { mcuCore, portDirRel });
    }
  }
  /* Include drivers/common when any driver is selected. The I2C/UART/ADC
     drivers #include "../common/stm32_wait.h", so without this every download
     that picks one of those fails to compile with a missing-header error. */
  const hasDriver = list.some((p) => p.zipPathPrefix.startsWith('drivers/') && p.zipPathPrefix !== 'drivers/common');
  if (hasDriver) {
    const drvCommonDir = path.join(codeDir, 'drivers', 'common');
    if (fs.existsSync(drvCommonDir) && !list.some((p) => p.zipPathPrefix === 'drivers/common')) {
      list.push({ zipPathPrefix: 'drivers/common', sourceDir: drvCommonDir });
    }
  }
  /* Include peripherals/common when any peripheral is selected (shared sensor_result.h) */
  const hasPeripheral = list.some((p) => p.zipPathPrefix.startsWith('peripherals/') && p.zipPathPrefix !== 'peripherals/common');
  if (hasPeripheral) {
    const commonDir = path.join(codeDir, 'peripherals', 'common');
    if (fs.existsSync(commonDir) && !list.some((p) => p.zipPathPrefix === 'peripherals/common')) {
      list.push({ zipPathPrefix: 'peripherals/common', sourceDir: commonDir });
    }
  }
  return list;
}

/**
 * Recursively list all files under dir (relative to dir).
 * @param {string} dir - absolute path
 * @param {string} relative - relative path prefix (e.g. 'startup')
 * @returns {Array<{ relativePath: string, absolutePath: string }>}
 */
function listFilesRecursive(dir, relative = '') {
  const out = [];
  let entries;
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch (err) {
    return out;
  }
  for (const e of entries) {
    const abs = path.join(dir, e.name);
    const rel = relative ? path.join(relative, e.name) : e.name;
    if (e.isDirectory()) {
      out.push(...listFilesRecursive(abs, rel));
    } else {
      out.push({ relativePath: rel.replace(/\\/g, '/'), absolutePath: abs });
    }
  }
  return out;
}

/* ===========================================================================
 * Generated build foundation.
 *
 * Downloads share a GNU startup, generated linker script and CMake build.
 * Exact memory profiles take precedence over unverified family templates.
 * Bare-metal builds use soft-float; FreeRTOS flags match the selected port.
 * Cross-compilation validates builds, not electrical or on-device behavior.
 * ========================================================================= */

/** Default GCC architecture flags; RTOS port flags are selected separately. */
function coreArchFlags(mcuCore) {
  const map = {
    'cortex-m0':     'cortex-m0',
    'cortex-m0plus': 'cortex-m0plus',
    'cortex-m3':     'cortex-m3',
    'cortex-m4':     'cortex-m4',
    'cortex-m7':     'cortex-m7',
    'cortex-m33':    'cortex-m33',
  };
  return map[mcuCore] || 'cortex-m4';
}

/** Conservative default flash/RAM per family (origins: FLASH 0x08000000, RAM
 *  0x20000000). These are DEFAULTS to make the download build — the header
 *  tells the user to set their exact part. Note: H7/H5/L5/U5 use a different
 *  RAM origin on some parts; the generated header flags that. */
function familyMemoryDefaults(family) {
  const f = String(family || '').toUpperCase().replace('+', 'PLUS');
  const table = {
    F0: [64, 8], F1: [128, 20], F2: [1024, 128], F3: [256, 48],
    F4: [512, 128], F7: [1024, 256], G0: [128, 36], G4: [512, 128],
    L0: [192, 20], L1: [256, 48], L4: [512, 128], L4PLUS: [1024, 320],
    WB: [1024, 256], WL: [256, 64], H5: [2048, 256], H7: [1024, 512],
    L5: [512, 256], U5: [2048, 256],
  };
  const [flashK, ramK] = table[f] || [256, 64];
  return { flashK, ramK };
}

/** GNU-as startup: vector table + C-runtime init + weak handlers. */
function buildStartupContent(mcuCore) {
  const cpu = coreArchFlags(mcuCore);
  return fs.readFileSync(path.join(defaultCodeDir(), 'core', cpu, 'startup/startup_stm32.s'), 'utf8');
}

/** GNU ld linker script with .isr_vector at flash base + RAM overflow guard. */
function buildLinkerContent(family, mcuName) {
  const profile = exactMemory(mcuName);
  const { flashK, ramK } = profile || familyMemoryDefaults(family);
  const fam = String(family || '').toUpperCase();
  const ramNote = /^(H7|H5|L5|U5)$/.test(fam)
    ? `\n * NOTE: some ${fam} parts place SRAM at a different origin (e.g. 0x24000000\n *       on H7 AXI SRAM). Verify ORIGIN(RAM) against your reference manual.`
    : '';
  return `/*
 * Linker script (STM32${fam}) — generated by Riviera Tech Solutions STM32 Suite.
 *
 * ${profile ? "Exact-part memory: " + mcuName + ". Source: " + profile.source : "UNVERIFIED FAMILY TEMPLATE: edit FLASH/RAM to match your exact part before flashing."}${ramNote}
 */
ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = ${flashK}K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = ${ramK}K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
__initial_sp = _estack;
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text*)
    *(.rodata*)
    *(.glue_7) *(.glue_7t)
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
  .ARM       : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH

  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /* Survives reset: the startup zeroes only .bss, so anything here keeps its
     value across a warm reset. Used by the optional fault handler to leave a
     crash record for the next boot. */
  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    *(.noinit*)
    . = ALIGN(4);
    _enoinit = .;
  } > RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM

  _sheap = _enoinit;
  _eheap = _estack - _Min_Stack_Size;
}
`;
}

/** CMakeLists that cross-compiles the whole download into firmware.bin/.hex. */
function buildCMakeContent(mcuCore, family, mcuName, hasFreeRTOS = false) {
  const cpu = coreArchFlags(mcuCore);
  // 'L4+' must become STM32L4PLUS — '+' is not a valid C macro character, and
  // the drivers' #if blocks test STM32L4PLUS.
  const fam = String(family || '').toUpperCase().replace('+', 'PLUS');
  return `cmake_minimum_required(VERSION 3.16)

# STM32${fam} (${mcuName || ''}) firmware — generated by Riviera Tech Solutions STM32 Suite.
# Build:  cmake -S . -B build -G Ninja && cmake --build build
# Output: build/firmware.elf, build/firmware.bin, build/firmware.hex
# (The sh rts.sh flash scripts drive exactly this.)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Resolve the toolchain by NAME (install it and add its bin folder to PATH).
# Setting a full path without the .exe suffix makes CMake reject the compiler on
# Windows, so let CMake find and suffix it per-platform.
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_SIZE         arm-none-eabi-size)

project(firmware C ASM)

set(MCU_FLAGS -mcpu=${cpu} -mthumb ${hasFreeRTOS && /cortex-m[47]$/.test(cpu) ? (cpu === "cortex-m7" ? "-mfpu=fpv5-d16 -mfloat-abi=hard" : "-mfpu=fpv4-sp-d16 -mfloat-abi=hard") : "-mfloat-abi=soft"})
add_compile_definitions(STM32${fam})

file(GLOB_RECURSE C_SOURCES   CONFIGURE_DEPENDS \${CMAKE_SOURCE_DIR}/core/*.c \${CMAKE_SOURCE_DIR}/drivers/*.c \${CMAKE_SOURCE_DIR}/peripherals/*.c \${CMAKE_SOURCE_DIR}/rtos/*.c)
list(FILTER C_SOURCES EXCLUDE REGEX "/examples/")
file(GLOB_RECURSE ASM_SOURCES CONFIGURE_DEPENDS \${CMAKE_SOURCE_DIR}/core/*.s)
# Starter application. Add additional application sources here.
set(APP_SOURCES \${CMAKE_SOURCE_DIR}/main.c)

file(GLOB_RECURSE ALL_HEADERS \${CMAKE_SOURCE_DIR}/*.h)
set(INCLUDE_DIRS "")
foreach(h \${ALL_HEADERS})
  get_filename_component(d \${h} DIRECTORY)
  list(APPEND INCLUDE_DIRS \${d})
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

set(LINKER_SCRIPT \${CMAKE_SOURCE_DIR}/core/linker.ld)

add_executable(firmware.elf \${C_SOURCES} \${ASM_SOURCES} \${APP_SOURCES})
target_include_directories(firmware.elf PRIVATE \${INCLUDE_DIRS})
target_compile_options(firmware.elf PRIVATE \${MCU_FLAGS} -Wall -Wextra -ffunction-sections -fdata-sections -Og -g3)
target_link_options(firmware.elf PRIVATE \${MCU_FLAGS} -T\${LINKER_SCRIPT} -nostartfiles -Wl,-e,Reset_Handler -Wl,--gc-sections -Wl,-Map=firmware.map --specs=nano.specs --specs=nosys.specs)

add_custom_command(TARGET firmware.elf POST_BUILD
  COMMAND \${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware.elf> firmware.bin
  COMMAND \${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:firmware.elf> firmware.hex
  COMMAND \${CMAKE_SIZE} $<TARGET_FILE:firmware.elf>)
`;
}

/**
 * Build zip and pipe to response stream.
 * @param {object} options - { mcu, nodeIds, treeData, codeDir }
 * @param {object} res - Express response (will set headers and pipe archive)
 * @param {string} downloadFilename - e.g. 'stm32_stm32f401.zip'
 */
function buildAndStreamZip(options, res, downloadFilename) {
  const { mcu, nodeIds, treeData, codeDir } = options;
  const ts = new Date().toISOString();
  log('buildAndStreamZip:start', { mcuId: mcu?.id, nodeCount: nodeIds?.length });

  const nodes = treeData?.nodes || {};
  const includedIds = collectNodeIdsToInclude(nodes, 'core', nodeIds || []);
  log('includedIds', { count: includedIds.size, ids: Array.from(includedIds).slice(0, 20) });

  const mcuCore = mcu?.core || 'cortex-m4';
  const mcuFamily = mcu?.family || '';
  const pathsToAdd = getPathsToAdd(nodes, includedIds, mcuCore, codeDir, mcuFamily);
  log('pathsToAdd', { count: pathsToAdd.length, paths: pathsToAdd.map((p) => p.zipPathPrefix) });

  res.setHeader('Content-Type', 'application/zip');
  res.setHeader('Content-Disposition', `attachment; filename="${downloadFilename}"`);

  const archive = archiver('zip', { zlib: { level: 6 } });
  archive.on('error', (err) => {
    log('archive:error', { message: err.message });
    if (!res.headersSent) res.status(500).end();
  });
  archive.on('end', () => {
    log('archive:end', { bytes: archive.pointer() });
  });
  archive.pipe(res);

  const hasFreeRTOS = includedIds.has('rtos_freertos');
  const freertosPortDir = hasFreeRTOS ? getFreeRTOSPortDir(mcuCore) : '';
  const generatedFreeRTOSConfig = hasFreeRTOS
    ? buildFreeRTOSConfigContent(mcuCore, mcuFamily, mcu?.name || mcu?.id || '')
    : '';
  const rtsConfig = buildRtsConfigContent(mcu);
  const rtsEnv = buildRtsEnvContent();
  const rtsPowerShell = buildRtsPowerShellContent();
  const rtsShell = buildRtsBashContent();
  const rtsCmd = buildRtsCmdWrapperContent();
  const preFlashChecklist = buildPreFlashChecklistContent();
  const agentFlashGuide = buildAgentFlashGuideContent(mcu);
  const readmeLines = [
    `STM32 Embedded C Code Suite – Riviera Tech`,
    `MCU: ${mcu?.name || mcu?.id} (${mcu?.family}, ${mcu?.core})`,
    `Generated: ${new Date().toISOString()}`,
    'Build tools on PATH: arm-none-eabi-gcc with newlib, CMake 3.16+, Ninja.',
    'Windows: .\\rts.cmd build. Linux/macOS: sh rts.sh build.',
    'Or: cmake -S . -B build -G Ninja && cmake --build build',
    'main.c increments a heartbeat counter for debugger inspection; no LED pin is assumed.',
    'Read AGENTS.md and PRE_FLASH_CHECKLIST.txt before flashing.',
    ``,
    `Contents: core/ (startup, system, CMSIS), selected drivers/peripherals${hasFreeRTOS ? ', and FreeRTOS (kernel + port for this MCU)' : ''}. ${hasFreeRTOS ? 'Includes a buildable heartbeat application; verify board configuration before flashing.' : ''}`,
    hasFreeRTOS
      ? [
          ``,
          `=== FreeRTOS quick start ===`,
          `1. Verify configCPU_CLOCK_HZ in rtos/freertos/config/FreeRTOSConfig.h (pre-filled from selected MCU family/core) matches your system clock (Hz).`,
          `2. Add include paths: rtos/freertos, rtos/freertos/include, rtos/freertos/config, rtos/freertos/portable/GCC/${freertosPortDir}.`,
          `3. Add sources: rtos/freertos/*.c, rtos/freertos/portable/MemMang/heap_4.c, rtos/freertos/portable/GCC/${freertosPortDir}/*.c (and *.s if present).`,
          `4. In main(): create tasks, then call vTaskStartScheduler().`,
          `Full step-by-step: see QUICKSTART_FREERTOS.txt in this zip.`,
        ].join('\r\n')
      : '',
    ``,
    `Define your MCU family in compiler options: e.g. -DSTM32F4 or -DSTM32G0 (see drivers/README.md for full list).`,
    ``,
    `Import this folder into your STM32 project. Adjust toolchain include/source paths and CMSIS/startup as needed.`,
    ``,
    `Flashing scaffold included in zip root: rts.json + rts.env + rts.ps1 + rts.sh + rts(.cmd).`,
    `IMPORTANT: configure board-specific settings first (board label, clock tree, pin mapping, probe path).`,
    `- Run "sh rts.sh doctor" (Windows: .\\rts.cmd doctor) to check installed build tools; no hardware is contacted.`,
    `- Run "sh rts.sh flash" for build + verify + flash + reset with deterministic commands.`,
    `- rts.env is authoritative; rts.json records generation metadata.`,
    `- Build and doctor work without hardware. Flash requires board and memory confirmation in rts.env.`,
    `- Agent-focused runbook is in AGENT_FLASH_GUIDE.md.`,
  ].filter(Boolean);
  archive.append(readmeLines.join('\r\n'), { name: 'README.txt' });

  if (hasFreeRTOS && freertosPortDir) {
    const quickstart = [
      `QUICKSTART: STM32 + FreeRTOS (Riviera Tech Solutions STM32 Suite)`,
      `MCU: ${mcu?.name || mcu?.id} (${mcu?.core}) – Port: ${freertosPortDir}`,
      ``,
      `Build the included heartbeat application with CMake and Ninja. This zip has: core (startup, system, linker), your selected drivers and peripherals, and FreeRTOS kernel with the correct GCC port for your MCU. Device-specific hardware configuration is required.`,
      ``,
      `--- Step 1: Set your system clock ---`,
      `Open rtos/freertos/config/FreeRTOSConfig.h`,
      `  #define configCPU_CLOCK_HZ  ( ( unsigned long ) YOUR_SYSCLK_HZ )`,
      `The value is pre-filled based on selected MCU family/core; adjust if your board clock differs.`,
      ``,
      `--- Step 2: Add to your project ---`,
      `Include paths (add to your IDE or Makefile):`,
      `  - rtos/freertos`,
      `  - rtos/freertos/include`,
      `  - rtos/freertos/config`,
      `  - rtos/freertos/portable/GCC/${freertosPortDir}`,
      ``,
      `Source files to compile:`,
      `  - All .c in rtos/freertos/ (tasks.c, queue.c, list.c, timers.c, event_groups.c, stream_buffer.c)`,
      `  - rtos/freertos/portable/MemMang/heap_4.c`,
      `  - All .c and .s in rtos/freertos/portable/GCC/${freertosPortDir}/`,
      ``,
      `--- Step 3: main() ---`,
      `Create your tasks, then call vTaskStartScheduler(). Never returns.`,
      ``,
      `--- Toolchains ---`,
      `STM32CubeIDE: Project → Properties → C/C++ Build → Settings → Include paths & Source locations.`,
      `The included startup and FreeRTOS port target GNU Arm. Other toolchains require their matching ports/startup.`,
      `Build: cmake -S . -B build -G Ninja && cmake --build build.`,
      ``,
      `--- Stack and heap ---`,
      `If you get stack overflows: increase task stack size in xTaskCreate (e.g. 256 or 512 words) or configMINIMAL_STACK_SIZE.`,
      `If pvPortMalloc fails: increase configTOTAL_HEAP_SIZE in FreeRTOSConfig.h.`,
      ``,
      `--- Notes ---`,
      `- SysTick is used by FreeRTOS for the tick; do not use it elsewhere.`,
      `- configMAX_SYSCALL_INTERRUPT_PRIORITY (in FreeRTOSConfig.h): interrupts with priority >= this can call FreeRTOS API.`,
      `- FreeRTOS (C) Real Time Engineers Ltd. https://www.FreeRTOS.org  MIT License.`,
    ].join('\r\n');
    archive.append(quickstart, { name: 'QUICKSTART_FREERTOS.txt' });

    const freertosReadme = [
      `FreeRTOS kernel + GCC port for your STM32 (Riviera Tech Solutions STM32 Suite).`,
      `Port: ${freertosPortDir}`,
      ``,
      `Config is auto-generated for selected MCU core. Set configCPU_CLOCK_HZ in config/FreeRTOSConfig.h.`,
      `Full steps: see QUICKSTART_FREERTOS.txt in the zip root.`,
      ``,
      `FreeRTOS (C) Real Time Engineers Ltd. https://www.FreeRTOS.org  MIT License.`,
    ].join('\r\n');
    archive.append(freertosReadme, { name: 'rtos/freertos/README.txt' });


  }

  const archivedNames = new Set();
  for (const entry of pathsToAdd) {
    const { zipPathPrefix, sourceDir, freertosKernelOnly } = entry;
    let files = [];
    if (freertosKernelOnly) {
      const entries = fs.readdirSync(sourceDir, { withFileTypes: true });
      for (const e of entries) {
        const abs = path.join(sourceDir, e.name);
        if (e.isFile() && (e.name.endsWith('.c') || e.name === 'LICENSE.md')) {
          files.push({ relativePath: e.name, absolutePath: abs });
        }
        if (e.isDirectory() && (e.name === 'include' || e.name === 'config')) {
          files.push(...listFilesRecursive(abs, e.name));
        }
      }
    } else {
      files = listFilesRecursive(sourceDir);
    }
    for (const { relativePath, absolutePath } of files) {
      const entryName = zipPathPrefix ? `${zipPathPrefix}/${relativePath}` : relativePath;
      if (hasFreeRTOS && entryName === 'rtos/freertos/config/FreeRTOSConfig.h') {
        continue;
      }
      /* The download uses the shared GNU startup and generated memory defaults. */
      if (/^core\/startup\/.*\.s$/i.test(entryName) || entryName === 'core/linker.ld') {
        continue;
      }
      if (archivedNames.has(entryName)) continue;
      archivedNames.add(entryName);
      archive.file(absolutePath, { name: entryName });
    }
  }

  if (!archivedNames.has('drivers/README.md')) {
    archive.file(path.join(codeDir, 'drivers/README.md'), { name: 'drivers/README.md' });
  }

  /* Generated build foundation: a GNU-as startup + linker for every download,
     and a CMakeLists for all projects (providing a consistent build
     system). This is what makes a download actually assemble, link, and flash. */
  archive.append(buildStartupContent(mcuCore), { name: 'core/startup/startup.s' });
  archive.append(buildLinkerContent(mcuFamily, mcu?.name || mcu?.id), { name: 'core/linker.ld' });
  archive.append(buildCMakeContent(mcuCore, mcuFamily, mcu?.name || mcu?.id || '', hasFreeRTOS), { name: 'CMakeLists.txt' });
  archive.append(buildMainContent(hasFreeRTOS), { name: 'main.c' });
  archive.append(agentFlashGuide, { name: 'AGENTS.md' });

  if (hasFreeRTOS && generatedFreeRTOSConfig) {
    archive.append(generatedFreeRTOSConfig, { name: 'rtos/freertos/config/FreeRTOSConfig.h' });
  }

  archive.append(rtsConfig, { name: 'rts.json' });
  archive.append(rtsEnv, { name: 'rts.env' });
  archive.append(rtsPowerShell, { name: 'rts.ps1' });
  archive.append(rtsShell, { name: 'rts.sh', mode: 0o755 });
  archive.append(rtsShell, { name: 'rts', mode: 0o755 });
  archive.append(rtsCmd, { name: 'rts.cmd' });
  archive.append(preFlashChecklist, { name: 'PRE_FLASH_CHECKLIST.txt' });
  archive.append(agentFlashGuide, { name: 'AGENT_FLASH_GUIDE.md' });

  archive.finalize();
  log('buildAndStreamZip:finalize', { ts });
}

module.exports = {
  defaultCodeDir,
  buildFreeRTOSConfigContent,
  collectNodeIdsToInclude,
  getPathsToAdd,
  listFilesRecursive,
  buildAndStreamZip,
  // Generated build foundation — exported so the build oracle / tests exercise
  // the exact same startup/linker/CMake a real download ships.
  coreArchFlags,
  familyMemoryDefaults,
  buildStartupContent,
  buildLinkerContent,
  buildCMakeContent,
};
