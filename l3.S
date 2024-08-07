.globl l3__log_fast // l3__log_fast(msg)
.extern l3_log
.extern __libc_single_threaded

l3__log_fast:
    mov %fs:l3_my_tid@tpoff,%eax // Fetch the TLS-stashed TID into %eax
    cmp $0, %eax            // Is the TID already stashed?
    jnz .tid_stashed        // yes: skip the call to gettid
    mov $186, %eax          // syscall number for gettid
    push %rcx
    syscall
    pop %rcx
    mov %eax, %fs:l3_my_tid@tpoff // store the TID in TLS.
.tid_stashed:
    mov %rax, %r10          // get the TID into %r10
    mov l3_log(%rip), %r8   // fetch ptr to the global l3_log into register r8
    mov $1, %r9             // prepare to increment the index
    cmpb $0, __libc_single_threaded(%rip) // Are we single-threaded?
    jne .single_threaded    // __libc_single_threaded != 0 so skip the lock prefix
    lock
.single_threaded:
    xadd %r9,(%r8)
    add $32, %r8            // point r8 at the beginning of the l3_log.slots array.
    and $0x3fff, %r9        // idx %= L3_NUM_SLOTS
    shl $5, %r9             // scale the index by sizeof(L3_ENTRY)
    add %r9, %r8            // point r8 at our entry in the slots array.
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
