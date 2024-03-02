.globl l3_log_fast // l3_log_fast(msg)
.extern l3_log
.extern mytid

l3_log_fast:
    mov %fs:0xfffffffffffffffc,%eax
    test %rax,%rax
    jnz .gotid
    mov $186, %rax
    syscall
    mov %eax, %fs:0xfffffffffffffffc
.gotid:
    mov l3_log(%rip), %rcx      // l3_log -> rcx

    mov  $1, %edx
    lock xadd %rdx, (%rcx)
    and $0x3fff, %edx           // idx -> rdx

    add $32, %rcx
    shl $5, %rdx
    add %rdx, %rcx
    mov %rax, (%rcx)  // tid
    add $8, %rcx
    mov %rdi, (%rcx)  // msg
    add $8, %rcx
    movq $0, (%rcx)  // arg1
    add $8, %rcx
    movq $0, (%rcx)  // arg1
    ret

