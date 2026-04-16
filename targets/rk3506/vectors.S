.syntax unified
.cpu cortex-a7
.arm

.section .text.vectors, "ax"
.balign 32
.global __vector_table_start
.global __vector_table_end
.type __vector_table_start, %object

.extern _start
.extern rk3506_handle_svc
.extern rk3506_handle_irq
.extern rk3506_handle_fiq
.extern rk3506_exception_fatal

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

__vector_table_start:
    b _start
    b rk3506_undefined_entry
    b rk3506_svc_entry
    b rk3506_prefetch_abort_entry
    b rk3506_data_abort_entry
    b rk3506_reserved_entry
    b rk3506_irq_entry
    b rk3506_fiq_entry
__vector_table_end:

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
    bl rk3506_exception_fatal
.endm

.global rk3506_svc_entry
.type rk3506_svc_entry, %function
rk3506_svc_entry:
    build_exception_frame EXC_SVC
    bl rk3506_handle_svc
    destroy_exception_frame
    movs pc, lr

.global rk3506_irq_entry
.type rk3506_irq_entry, %function
rk3506_irq_entry:
    build_exception_frame EXC_IRQ
    bl rk3506_handle_irq
    destroy_exception_frame
    subs pc, lr, #4

.global rk3506_fiq_entry
.type rk3506_fiq_entry, %function
rk3506_fiq_entry:
    build_exception_frame EXC_FIQ
    bl rk3506_handle_fiq
    destroy_exception_frame
    subs pc, lr, #4

fatal_vector rk3506_undefined_entry, EXC_UNDEFINED
fatal_vector rk3506_prefetch_abort_entry, EXC_PREFETCH_ABORT
fatal_vector rk3506_data_abort_entry, EXC_DATA_ABORT
fatal_vector rk3506_reserved_entry, EXC_RESERVED
