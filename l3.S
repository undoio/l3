.globl l3__log_fast // l3__log_fast(msg)
.extern l3_log

l3__log_fast:
    mov %fs:0xfffffffffffffffc,%eax // Fetch the TLS-stashed TID into %eax
    test %rax,%rax          // Did we already stash something?
    jnz .gotid              //      Yes: avoid the syscall because doing so is slow.
    mov $186, %rax          //      No: 1. do the syscall. First mov __NR_gettid into %rax (i.e. the syscall number)
    push %rcx               //          2. preserve $rcx because the syscall can trash it.
    syscall                 //          3. do the actual syscall
    pop %rcx                //          4. restore $rcx which the syscall will have trashed.
    mov %eax, %fs:0xfffffffffffffffc // 5. and stuff the tid in TLS so that we don't need to do the syscall next time.
.gotid:
    mov l3_log(%rip), %r8   // fetch ptr to the global l3_log into register r8
    mov  $1, %r9
    lock xadd %r9, (%r8)    // atomically increment-and-fetch the index field in l3_log.
    and $0x3fff, %r9        // idx %= L3_NUM_SLOTS
    add $32, %r8            // point r8 at the beginning of the l3_log.slots array.
    shl $5, %r9             // scale the index by sizeof(L3_ENTRY)
    add %r9, %r8            // point r9 at our entry in the slots array.
    mov %eax, (%r8)         // The tid is in %eax from the call to to gettid above.
#ifdef L3_LOC_ENABLED
    add $4, %r8             // Point r8 at the loc field of the slot.
    mov %edi, (%r8)         // Stash the LOC value.
    add $4, %r8             // Point r8 at the msg field of the slot.
#else
    add $8, %r8             // Point r8 at the msg field of the slot.
#endif // L3_LOC_ENABLED
    mov %rsi, (%r8)         // Stash the msg arg in the slot.
    add $8, %r8             // Point r8 at the arg1 field in the slot.
    movq %rdx, (%r8)        // Stash arg1 in the slot
    add $8, %r8             // Point r8 at the arg2 field in the slot.
    movq %rcx, (%r8)        // Stash arg2 in the slot.
    ret
