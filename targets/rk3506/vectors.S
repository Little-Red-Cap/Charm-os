.syntax unified
.cpu cortex-a7
.arm

.section .text.vectors, "ax"
.balign 32
.global __vector_table_start
.global __vector_table_end
.type __vector_table_start, %object

.extern _start

__vector_table_start:
    b _start
    b rk3506_vector_hold
    b rk3506_vector_hold
    b rk3506_vector_hold
    b rk3506_vector_hold
    b rk3506_vector_hold
    b rk3506_vector_hold
    b rk3506_vector_hold
__vector_table_end:

.type rk3506_vector_hold, %function
rk3506_vector_hold:
    wfe
    b rk3506_vector_hold

.size rk3506_vector_hold, . - rk3506_vector_hold
