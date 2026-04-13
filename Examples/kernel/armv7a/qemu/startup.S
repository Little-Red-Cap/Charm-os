.syntax unified
.cpu cortex-a7

.section .text.boot, "ax"
.global _start
.type _start, %function
.extern main
.extern __bss_start
.extern __bss_end
.extern __stack_top

_start:
    b reset_handler
    b .
    b .
    b .
    b .
    b .
    b .
    b .

reset_handler:
    cpsid if
    ldr sp, =__stack_top

    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:
    cmp r0, r1
    bhs 2f
    str r2, [r0], #4
    b 1b

2:
    bl main

3:
    wfe
    b 3b

.size _start, . - _start
