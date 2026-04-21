.syntax unified
.cpu cortex-a7

.section .text, "ax"
.global armv7a_invoke_handoff_launch_probe
.global armv7a_handoff_launch_probe_return_site
.type armv7a_invoke_handoff_launch_probe, %function
.extern armv7a_handoff_launch_probe_target

// r0 = probe target
// r1 = next-image branch target
// r2 = arg0 handoff pointer
// r3 = stack pointer to inherit
armv7a_invoke_handoff_launch_probe:
    push {r4, r5, r6, lr}
    mov r4, sp
    mov ip, r0
    mov sp, r3
    mov r0, r2
    blx ip
armv7a_handoff_launch_probe_return_site:
    cmp r0, #0
    movne r0, #1
    pop {r4, r5, r6, pc}

.size armv7a_invoke_handoff_launch_probe, . - armv7a_invoke_handoff_launch_probe

.global armv7a_handoff_launch_probe_target
.type armv7a_handoff_launch_probe_target, %function
.extern armv7a_record_handoff_launch_probe

armv7a_handoff_launch_probe_target:
    mov r2, sp
    mrs r3, cpsr
    mov sp, r4
    push {r4, lr}
    sub sp, sp, #8
    str lr, [sp]
    bl armv7a_record_handoff_launch_probe
    add sp, sp, #8
    pop {r4, lr}
    mov r0, #1
    bx lr

.size armv7a_handoff_launch_probe_target, . - armv7a_handoff_launch_probe_target
