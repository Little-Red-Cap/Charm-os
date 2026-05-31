.syntax unified
.cpu cortex-m7
.thumb

.section .text.charm_app_main, "ax", %progbits
.global charm_app_main
.type charm_app_main, %function
charm_app_main:
    push {r4, r5, r6, lr}
    mov r4, r0
    mov r5, r1
    mov r6, r2

    cmp r4, #0
    beq .Lbad
    cmp r5, #3
    bne .Lbad
    cmp r6, #0
    beq .Lbad

    ldr r0, [r4, #20]
    cbz r0, .Lbad
    blx r0

    ldr r3, [r4, #16]
    cbz r3, .Lbad
    adr r0, message
    movs r1, #message_len
    blx r3
    cmp r0, #0
    blt .Lbad

    ldr r3, [r4, #60]
    cbz r3, .Lreturn_ok
    movs r0, #0
    blx r3

.Lreturn_ok:
    movs r0, #0
    pop {r4, r5, r6, pc}

.Lbad:
    movs r0, #77
    pop {r4, r5, r6, pc}

    .align 2
message:
.ascii "modulex_hello_app: charm_app_main entered\n"
.set message_len, . - message

.section .rodata.modulex_hello, "a", %progbits
.byte 0
