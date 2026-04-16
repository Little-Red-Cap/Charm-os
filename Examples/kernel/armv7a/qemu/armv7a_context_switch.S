.syntax unified
.cpu cortex-a7

.section .text, "ax"

.global armv7a_context_switch
.type armv7a_context_switch, %function
armv7a_context_switch:
    push {r4-r11, ip, lr}
    str sp, [r0]
    mov sp, r1
    pop {r4-r11, ip, lr}
    mov r0, #1
    bx lr
.size armv7a_context_switch, . - armv7a_context_switch

.global armv7a_context_thread_trampoline
.type armv7a_context_thread_trampoline, %function
armv7a_context_thread_trampoline:
    mov lr, ip
    mov r0, r5
    bx r4
.size armv7a_context_thread_trampoline, . - armv7a_context_thread_trampoline
