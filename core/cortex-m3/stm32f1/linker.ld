/* Minimal linker script for STM32F1. Adjust LENGTH for your part (e.g. F103: 128K/20K). */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 128K
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 20K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
__initial_sp = _estack;
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) } > FLASH
  .data : { . = ALIGN(4); *(.data*) . = ALIGN(4); } > RAM AT > FLASH
  .bss  : { . = ALIGN(4); *(.bss*) *(COMMON) . = ALIGN(4); } > RAM
  _sidata = LOADADDR(.data);
  _sdata = ADDR(.data);
  _edata = _sdata + SIZEOF(.data);
  _sbss = ADDR(.bss);
  _ebss = _sbss + SIZEOF(.bss);
  /* Retained crash record is outside startup-cleared BSS and heap. */
  .noinit (NOLOAD) : { . = ALIGN(4); *(.noinit*) . = ALIGN(4); _enoinit = .; } > RAM
  _sheap = _enoinit;
  _eheap = _estack;
}
