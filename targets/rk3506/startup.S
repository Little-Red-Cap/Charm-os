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
.extern __svc_stack_top
.extern __irq_stack_top
.extern __fiq_stack_top
.extern __abt_stack_top
.extern __und_stack_top
.extern __sys_stack_top
.extern __stack_top

.equ MODE_FIQ, 0x11
.equ MODE_IRQ, 0x12
.equ MODE_SVC, 0x13
.equ MODE_ABT, 0x17
.equ MODE_UND, 0x1B
.equ MODE_SYS, 0x1F

.macro enter_mode mode
    mrs r1, cpsr
    bic r1, r1, #0x1f
    orr r1, r1, #\mode
    msr cpsr_c, r1
.endm

_start:
    mrs r3, cpsr
    orr r4, r3, #0x1c0
    msr cpsr_fsxc, r4
    isb

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
    bl init_mode_stacks

    mov r0, r3
    bl rk3506_platform_reset_early
    ldr r0, =__vector_table_start
    bl rk3506_platform_install_exception_vectors

    enter_mode MODE_SYS
    ldr sp, =__sys_stack_top
    bl rk3506_boot_main

3:
    wfe
    b 3b

init_mode_stacks:
    ldr r0, =__und_stack_top
    enter_mode MODE_UND
    mov sp, r0

    ldr r0, =__abt_stack_top
    enter_mode MODE_ABT
    mov sp, r0

    ldr r0, =__irq_stack_top
    enter_mode MODE_IRQ
    mov sp, r0

    ldr r0, =__fiq_stack_top
    enter_mode MODE_FIQ
    mov sp, r0

    ldr r0, =__svc_stack_top
    enter_mode MODE_SVC
    mov sp, r0
    bx lr

.size _start, . - _start
