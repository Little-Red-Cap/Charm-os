.syntax unified
.cpu cortex-a7

.section .text.boot, "ax"
.global reset_handler
.type reset_handler, %function
.extern main
.extern armv7a_vector_table
.extern __bss_start
.extern __bss_end
.extern __svc_stack_top
.extern __irq_stack_top
.extern __fiq_stack_top
.extern __abt_stack_top
.extern __und_stack_top
.extern __sys_stack_top

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

reset_handler:
    cpsid if
    ldr sp, =__svc_stack_top

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

    ldr r0, =armv7a_vector_table
    mcr p15, 0, r0, c12, c0, 0
    dsb
    isb

    enter_mode MODE_SYS
    ldr sp, =__sys_stack_top
    bl main

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

.size reset_handler, . - reset_handler
