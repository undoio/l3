.globl l3__log_fast // l3__log_fast(msg)
.extern l3_log

l3__log_fast:
    mov %fs:0xfffffffffffffffc,%eax
    test %rax,%rax
    jnz .gotid
    mov $186, %rax
    syscall
    mov %eax, %fs:0xfffffffffffffffc
.gotid:
    mov l3_log(%rip), %r8      // l3_log -> r8

    mov  $1, %r9
    lock xadd %r9, (%r8)
    and $0x3fff, %r9           // idx -> r9

    add $32, %r8
    shl $5, %r9
    add %r9, %r8
    mov %rax, (%r8)  // tid
    add $8, %r8
    mov %rdi, (%r8)  // msg
    add $8, %r8
    movq $0, (%r8)  // arg1
    add $8, %r8
    movq $0, (%r8)  // arg2
    ret
