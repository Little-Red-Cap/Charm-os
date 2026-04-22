.syntax unified
.cpu cortex-a7

.section .text, "ax"
.global armv7a_invoke_handoff_launch_live_trampoline
.global armv7a_handoff_live_trampoline_target
.global armv7a_handoff_live_stage_entry
.global armv7a_handoff_live_reinit_stacks
.type armv7a_invoke_handoff_launch_live_trampoline, %function
.type armv7a_handoff_live_trampoline_target, %function
.type armv7a_handoff_live_stage_entry, %function
.type armv7a_handoff_live_reinit_stacks, %function

.extern armv7a_handoff_live_stage_main
.extern armv7a_handoff_runtime_stage_main
.extern __und_stack_top
.extern __abt_stack_top
.extern __irq_stack_top
.extern __fiq_stack_top
.extern __svc_stack_top
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

// r0 = dispatch target
// r1 = next-stage target
// r2 = arg0 handoff pointer
// r3 = inherited stack pointer
armv7a_invoke_handoff_launch_live_trampoline:
    mov ip, r0
    mov sp, r3
    mov r0, r1
    mov r1, r2
    bx ip

.size armv7a_invoke_handoff_launch_live_trampoline, . - armv7a_invoke_handoff_launch_live_trampoline

// r0 = next-stage target
// r1 = arg0 handoff pointer
armv7a_handoff_live_trampoline_target:
    mov ip, r0
    mov r0, r1
    bx ip

.size armv7a_handoff_live_trampoline_target, . - armv7a_handoff_live_trampoline_target

// r0 = arg0 handoff pointer
armv7a_handoff_live_stage_entry:
    mov r1, sp
    mrs r2, cpsr
    b armv7a_handoff_live_stage_main

.size armv7a_handoff_live_stage_entry, . - armv7a_handoff_live_stage_entry

// r0 = handoff pointer
armv7a_handoff_live_reinit_stacks:
    mov r4, r0

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

    ldr r0, =__sys_stack_top
    enter_mode MODE_SYS
    mov sp, r0

    mov r0, r4
    b armv7a_handoff_runtime_stage_main

.size armv7a_handoff_live_reinit_stacks, . - armv7a_handoff_live_reinit_stacks
