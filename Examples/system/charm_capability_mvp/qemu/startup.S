.syntax unified
.cpu cortex-a15
.arm

.section .text.start, "ax", %progbits
.global _start
.type _start, %function
_start:
    ldr sp, =_stack_top
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
1:
    cmp r0, r1
    strlo r2, [r0], #4
    blo 1b
    bl main
2:
    wfi
    b 2b
.size _start, . - _start
