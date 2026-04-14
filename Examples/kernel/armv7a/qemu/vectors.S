.syntax unified
.cpu cortex-a7

.section .text.vectors, "ax"
.align 5

.global _start
.global armv7a_vector_table
.type _start, %function
.type armv7a_vector_table, %function

.extern reset_handler
.extern armv7a_handle_svc
.extern armv7a_exception_fatal

.equ EXC_UNDEFINED, 1
.equ EXC_PREFETCH_ABORT, 2
.equ EXC_DATA_ABORT, 3
.equ EXC_RESERVED, 4
.equ EXC_IRQ, 5
.equ EXC_FIQ, 6

_start:
armv7a_vector_table:
    b reset_handler
    b armv7a_undefined_entry
    b armv7a_svc_entry
    b armv7a_prefetch_abort_entry
    b armv7a_data_abort_entry
    b armv7a_reserved_entry
    b armv7a_irq_entry
    b armv7a_fiq_entry

.macro fatal_vector name, id
    .global \name
    .type \name, %function
\name:
    push {r0-r3, r12, lr}
    mov r0, #\id
    mov r1, lr
    mrs r2, spsr
    bl armv7a_exception_fatal
.endm

.global armv7a_svc_entry
.type armv7a_svc_entry, %function
armv7a_svc_entry:
    push {r0-r3, r12, lr}
    mov r0, lr
    mrs r1, spsr
    bl armv7a_handle_svc
    pop {r0-r3, r12, lr}
    movs pc, lr

fatal_vector armv7a_undefined_entry, EXC_UNDEFINED
fatal_vector armv7a_prefetch_abort_entry, EXC_PREFETCH_ABORT
fatal_vector armv7a_data_abort_entry, EXC_DATA_ABORT
fatal_vector armv7a_reserved_entry, EXC_RESERVED
fatal_vector armv7a_irq_entry, EXC_IRQ
fatal_vector armv7a_fiq_entry, EXC_FIQ

.size _start, . - _start
