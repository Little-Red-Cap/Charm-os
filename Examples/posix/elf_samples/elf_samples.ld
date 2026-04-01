ENTRY(entry)

ELF_BASE = 0x20080000;

PHDRS
{
  host PT_LOAD FLAGS(6);
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = ELF_BASE;
  .hostcall : {
    KEEP(*(.hostcall))
  } :host

  . = ELF_BASE + 0x100;
  .text : {
    *(.text*)
  } :text
  .rodata : {
    *(.rodata*)
  } :text
  . = ALIGN(0x100);
  .data : {
    *(.data*)
  } :data
  .bss : {
    *(.bss*)
    *(COMMON)
  } :data
}
