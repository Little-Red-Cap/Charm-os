ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x40200000, LENGTH = 126M
}

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : ALIGN(32)
    {
        KEEP(*(.text.vectors))
        KEEP(*(.text.boot))
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > RAM :text

    .data : ALIGN(16)
    {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    } > RAM :data

    .bss (NOLOAD) : ALIGN(16)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM :data

    .stacks (NOLOAD) : ALIGN(16)
    {
        __stack_region_start = .;

        . += 0x0400;
        __und_stack_top = .;

        . += 0x0400;
        __abt_stack_top = .;

        . += 0x0400;
        __irq_stack_top = .;

        . += 0x0400;
        __fiq_stack_top = .;

        . += 0x0800;
        __svc_stack_top = .;

        . += 0x1000;
        __sys_stack_top = .;

        __stack_top = __sys_stack_top;
        __stack_region_end = .;
    } > RAM :data

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
