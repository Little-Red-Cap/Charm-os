ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 16M
  RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 1M
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
  } > FLASH

  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  _sidata = LOADADDR(.data);
  .data : AT(ADDR(.ARM.exidx) + SIZEOF(.ARM.exidx))
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM

  .elf_load 0x20080000 (NOLOAD) :
  {
    . = ALIGN(16);
    __elf_load_start = .;
    KEEP(*(.elf_load))
    KEEP(*(.elf_load*))
    . = ALIGN(16);
    __elf_load_end = .;
  } > RAM

  .bss (NOLOAD) :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM

  _end = .;
  end = .;
  __end__ = .;
}
