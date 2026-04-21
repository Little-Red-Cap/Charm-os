.syntax unified
.cpu cortex-a7

.section .text, "ax"
.global armv7a_invoke_handoff_launch_live_trampoline
.global armv7a_handoff_live_trampoline_target
.global armv7a_handoff_live_stage_entry
.type armv7a_invoke_handoff_launch_live_trampoline, %function
.type armv7a_handoff_live_trampoline_target, %function
.type armv7a_handoff_live_stage_entry, %function

.extern armv7a_handoff_live_stage_main

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
