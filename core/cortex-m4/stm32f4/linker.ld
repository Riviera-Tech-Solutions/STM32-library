/*
 * Linker script for STM32F4 (GNU ld).
 *
 * Adjust the two LENGTH values below to your exact part:
 *   STM32F401RE : FLASH 512K, RAM 96K
 *   STM32F407VG : FLASH 1024K, RAM 128K  (+64K CCM at 0x10000000, not mapped here)
 *   STM32F411   : FLASH 512K, RAM 128K
 * Flash origin (0x08000000) and RAM origin (0x20000000) are the same across F4.
 */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Top of stack = end of RAM (full-descending stack). */
_estack = ORIGIN(RAM) + LENGTH(RAM);
__initial_sp = _estack;

/* Minimum heap/stack the image reserves; the build fails if RAM can't hold it. */
_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

SECTIONS
{
  /* Vector table FIRST in flash, so it lands at 0x08000000 where the core
     fetches the initial SP and reset vector. KEEP() stops --gc-sections from
     discarding it (nothing "calls" it). */
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

  /* ARM exception unwind tables — harmless to keep, avoids link warnings. */
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
  .ARM       : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH

  /* Load address of .data in FLASH — Reset_Handler copies from here. */
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

  /* Fail the link if RAM can't fit .data + .bss + the reserved heap/stack,
     instead of silently overflowing at runtime. */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM

  /* Retained crash record is outside startup-cleared BSS and heap. */
  .noinit (NOLOAD) : { . = ALIGN(4); *(.noinit*) . = ALIGN(4); _enoinit = .; } > RAM
  _sheap = _enoinit;
  _eheap = _estack;
}
