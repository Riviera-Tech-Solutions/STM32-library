/* Minimal linker script for STM32H7 (single bank). Adjust for dual bank / your part. */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1M
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 512K
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
