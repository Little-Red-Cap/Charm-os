.syntax unified
.cpu cortex-a7

.section .text.vectors, "ax"
.align 5

.global _start
.global armv7a_vector_table
.type _start, %function
.type armv7a_vector_table, %function

.extern reset_handler
.extern armv7a_handle_irq
.extern armv7a_handle_svc
.extern armv7a_exception_fatal

.equ EXC_UNDEFINED, 1
.equ EXC_PREFETCH_ABORT, 2
.equ EXC_DATA_ABORT, 3
.equ EXC_RESERVED, 4
.equ EXC_IRQ, 5
.equ EXC_FIQ, 6
.equ EXC_SVC, 7

.equ FRAME_SPSR, 0
.equ FRAME_VECTOR_ID, 4
.equ FRAME_SIZE, 32

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

.macro build_exception_frame id
    push {r0-r3, r12, lr}
    sub sp, sp, #8
    mrs r1, spsr
    str r1, [sp, #FRAME_SPSR]
    mov r1, #\id
    str r1, [sp, #FRAME_VECTOR_ID]
    mov r0, sp
.endm

.macro destroy_exception_frame
    add sp, sp, #8
    pop {r0-r3, r12, lr}
.endm

.macro fatal_vector name, id
    .global \name
    .type \name, %function
\name:
    build_exception_frame \id
    bl armv7a_exception_fatal
.endm

.global armv7a_svc_entry
.type armv7a_svc_entry, %function
armv7a_svc_entry:
    build_exception_frame EXC_SVC
    bl armv7a_handle_svc
    destroy_exception_frame
    movs pc, lr

.global armv7a_irq_entry
.type armv7a_irq_entry, %function
armv7a_irq_entry:
    build_exception_frame EXC_IRQ
    bl armv7a_handle_irq
    destroy_exception_frame
    subs pc, lr, #4

fatal_vector armv7a_undefined_entry, EXC_UNDEFINED
fatal_vector armv7a_prefetch_abort_entry, EXC_PREFETCH_ABORT
fatal_vector armv7a_data_abort_entry, EXC_DATA_ABORT
fatal_vector armv7a_reserved_entry, EXC_RESERVED
fatal_vector armv7a_fiq_entry, EXC_FIQ

.size _start, . - _start
