.syntax unified
.cpu cortex-a7
.arm

.section .text.boot, "ax"
.global _start
.type _start, %function

.extern rk3506_boot_main
.extern rk3506_platform_reset_early
.extern rk3506_platform_install_exception_vectors
.extern __vector_table_start
.extern __bss_start
.extern __bss_end
.extern __stack_top

_start:
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
    bl rk3506_platform_reset_early
    ldr r0, =__vector_table_start
    bl rk3506_platform_install_exception_vectors
    bl rk3506_boot_main

3:
    wfe
    b 3b

.size _start, . - _start
