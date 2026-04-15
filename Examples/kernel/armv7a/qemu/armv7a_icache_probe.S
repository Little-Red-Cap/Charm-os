.syntax unified
.arm

.section .text.armv7a_icache_probe_target_a, "ax", %progbits
.global armv7a_icache_probe_target_a
.type armv7a_icache_probe_target_a, %function
.balign 4096
armv7a_icache_probe_target_a:
    mov r0, #0xA1
    bx lr
.size armv7a_icache_probe_target_a, . - armv7a_icache_probe_target_a

.section .text.armv7a_icache_probe_target_b, "ax", %progbits
.global armv7a_icache_probe_target_b
.type armv7a_icache_probe_target_b, %function
.balign 4096
armv7a_icache_probe_target_b:
    mov r0, #0xB2
    bx lr
.size armv7a_icache_probe_target_b, . - armv7a_icache_probe_target_b
