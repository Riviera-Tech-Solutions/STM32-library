/* Minimal linker script for STM32F3. Adjust LENGTH for your part. */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 256K
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 48K
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
