ENTRY(charm_app_main)

ELF_BASE = 0x24070000;

PHDRS
{
  text PT_LOAD FLAGS(5);
  rodata PT_LOAD FLAGS(4);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = ELF_BASE;
  .text : {
    *(.text*)
  } :text

  .rodata : {
    *(.rodata*)
  } :rodata

  . = ALIGN(0x100);
  .data : {
    *(.data*)
  } :data

  .bss : {
    *(.bss*)
    *(COMMON)
  } :data
}
