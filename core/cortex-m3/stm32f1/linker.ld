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
  .text : { *(.text*) *(.rodata*) } > FLASH
  .data : { *(.data*) } > RAM AT > FLASH
  .bss  : { *(.bss*) *(COMMON) } > RAM
  _sidata = LOADADDR(.data);
  _sdata = ADDR(.data);
  _edata = _sdata + SIZEOF(.data);
  _sbss = ADDR(.bss);
  _ebss = _sbss + SIZEOF(.bss);
  _sheap = _ebss;
  _eheap = _estack;
}
