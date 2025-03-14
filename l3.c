/**
 * *****************************************************************************
 * \file l3.c
 * \author Greg Law
 * \brief L3: Lightweight Logging Library Implementation
 * \version 0.1
 * \date 2023-12-24
 *
 * \copyright Copyright (c) 2023
 * *****************************************************************************
 */
#include <dlfcn.h> // Makefile supplies required -D_GNU_SOURCE flag.

#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <stdio.h>

#if __APPLE__
#include <mach-o/getsect.h>
#include <pthread.h>
#else
#include <threads.h>
#include <sys/single_threaded.h>
#endif  // __APPLE__

#include <string.h>
#include <assert.h>

#include "l3.h"

#ifdef L3_LOC_ENABLED
#include "loc.h"
#endif  // L3_LOC_ENABLED

#if __APPLE__
#define L3_THREAD_LOCAL __thread
#define L3_GET_TID()    pthread_mach_thread_np(pthread_self())
#else
#define L3_THREAD_LOCAL thread_local
#define L3_GET_TID()  syscall(SYS_gettid)
#endif  // __APPLE__

/**
 * L3 Log entry Structure definitions:
 */
typedef union l3_entry
{
    struct l3_arguments
    {
        uint64_t args[3];
    } body;

    struct l3_footer
    {
        pid_t       tid;
    #ifdef L3_LOC_ENABLED
        loc_t       loc;
    #else
        uint32_t    loc;
    #endif  // L3_LOC_ENABLED
        const char *msg;
        uint64_t    argN;
    } foot;
} L3_ENTRY;

/**
 * Cross-check LOC's data structures. We need this to be true to ensure
 * that the sizeof(l3_entry) is unchanged w/LOC ON or OFF.
 */
#ifdef L3_LOC_ENABLED
L3_STATIC_ASSERT(sizeof(loc_t) == sizeof(uint32_t),
                 "Expected sizeof(loc_t) == 4 bytes.");
#endif  // L3_LOC_ENABLED

/**
 * Definitions for L3_LOG.{platform, loc_type} fields. This field is used to
 * reliably identify the provenance of a L3-log file so that the appropriate
 * tool / technique can be used to unpack log-entries when dumping the contents
 * of a L3-log file.
 */
typedef uint8_t platform_u8_t;
enum platform_t
{
      L3_LOG_PLATFORM_LINUX             =  ((uint8_t) 1)
    , L3_LOG_PLATFORM_MACOSX            // ((uint8_t) 2)
};

typedef uint8_t loc_type_u8_t;
enum loc_type_t
{
      L3_LOG_LOC_NONE                   =  ((uint8_t) 0)
    , L3_LOG_LOC_ENCODING               // ((uint8_t) 1)
    , L3_LOG_LOC_ELF_ENCODING           // ((uint8_t) 2)
};

/**
 * L3 Log Structure definitions:
 */
typedef struct l3_log
{
    uint64_t        idx; // Offset into `slots` for where the next entry is to be written. Atomic.
    uint64_t        fbase_addr;
    uint32_t        slot_count; // == L3_MAX_SLOTS. For determining the size of the ring buffer when decoding.
    uint16_t        pad2;
    uint8_t         platform;
    uint8_t         loc_type;
    uint64_t        pad1;
    L3_ENTRY        slots[0];
} L3_LOG;


L3_LOG *l3_log = NULL;      // L3_LOG_MMAP: Also referenced in l3.S.

#if __APPLE__

/**
 * Attempted solution to grab 'offset' of 'Section __cstring' at run-time.
 * But this is not quite working. Instead, we "fix" this issue, in the script
 * l3_dump.py, while unpacking the log-entries by grabbing the required offset
 * using Mac's 'size' binary.
 */
intptr_t
getBaseAddress() {
    Dl_info info;
    if (!dladdr((void *)getBaseAddress, &info)) {
        errno = L3_DLADDR_NOT_FOUND_ERRNO;
        return -1;
    }
    return  (intptr_t)info.dli_fbase;
}
#endif  // __APPLE__

L3_THREAD_LOCAL pid_t l3_my_tid = 0;

/**
 * ****************************************************************************
 * L3's default logging sub-system, using mmap()'ed files.
 */
int
l3_init(const char *path)
{
    uint64_t fsize = sizeof(L3_LOG) + L3_MAX_SLOTS * sizeof(L3_ENTRY);
    int fd = -1;
    if (path)
    {
        fd = open(path, O_RDWR | O_CREAT, 0666);
        if (fd == -1) {
            return -1;
        }
        if (lseek(fd, fsize, SEEK_SET) < 0) {
            return -1;
        }
        if (write(fd, &fd, 1) != 1) {
            return -1;
        }
    }

    l3_log = (L3_LOG *) mmap(NULL, fsize, PROT_READ|PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (l3_log == MAP_FAILED) {
        return -1;
    }

    // Technically, this is not needed as mmap() is guaranteed to return
    // zero-filled pages. We do this just to be clear where the idx begins.
    l3_log->idx = 0;
    l3_log->slot_count = L3_MAX_SLOTS;

#if __APPLE__
     l3_log->fbase_addr = getBaseAddress();
     l3_log->platform = L3_LOG_PLATFORM_MACOSX;
#else
    /* Linux: Let's find where rodata is loaded. */
    Dl_info info;
    if (!dladdr("test string", &info))
    {
        errno = L3_DLADDR_NOT_FOUND_ERRNO;
        return -1;
    }
    l3_log->fbase_addr = (intptr_t) info.dli_fbase;
    l3_log->platform = L3_LOG_PLATFORM_LINUX;
#endif  // __APPLE__

    // Note down, in the log-header, the type of LOC-encoding in effect.
    // LOC-encoding scheme is a compile-time choice when the application using
    // L3-logging is built. It cannot be changed at run-time as the encoding
    // schemes are mutually exclusive.
    // CFLAGS are setup as -DL3_LOC_ENABLED -DL3_LOC_ELF_ENABLED, for the case
    // of L3_LOC_ENABLED=2. So, check L3_LOC_ELF_ENABLED first.
    l3_log->loc_type = L3_LOG_LOC_NONE;
#if L3_LOC_ELF_ENABLED
    l3_log->loc_type = L3_LOG_LOC_ELF_ENCODING;
#elif L3_LOC_ENABLED
    l3_log->loc_type = L3_LOG_LOC_ENCODING;
#endif  // L3_LOC_ELF_ENABLED

    // printf("fbase_addr=%" PRIu64 " (0x%llx)\n", l3_log->fbase_addr, l3_log->fbase_addr);
    // printf("sizeof(L3_LOG)=%ld, header=%lu bytes\n",
    //        sizeof(L3_LOG), offsetof(L3_LOG,slots));
    return 0;
}

// ****************************************************************************

/**
 * l3_log_fn() - 'C' implementation of L3-logging.
 *
 * 'loc' is an argument synthesized by the caller-macro, under conditional.
 * This makes it possible to define DEBUG version of the caller-macro using printf(),
 * for argument v/s print-format specifiers in 'msg'.
 */
void
#ifdef L3_LOC_ENABLED
l3_log_fn(const char *msg, loc_t loc, int nargs, ...)
#else
l3_log_fn(const char *msg, uint32_t loc, int nargs, ...)
#endif
{
    const uint64_t args_per_block = sizeof(L3_ENTRY) / sizeof(uint64_t);
    uint64_t entryBlocks = 1 + (nargs + args_per_block - 1) / args_per_block;

    uint32_t ofs;
#ifndef  __APPLE__
    if (__libc_single_threaded)
    {
        ofs = l3_log->idx;
        l3_log->idx += entryBlocks; // the next log will write here
    } else
#endif  // __APPLE__
    {
        ofs = __sync_fetch_and_add(&l3_log->idx, entryBlocks);
    }

    if (!l3_my_tid) {
        l3_my_tid = L3_GET_TID();
    }

    ofs %= L3_MAX_SLOTS;
    // printf("Logging %ld blocks from idx %ld.\n", entryBlocks, ofs);

    // Write the blocks. The one always fits, but the rest might need to wrap.
    // Because 3 arguments fit in the span of the Entry's footer metadata, the byte layout may look like:
    // [ofs+0] = Args 0-2, [ofs+1] = Args 3-5, [ofs+N] = Footer
    va_list args;
    va_start(args, nargs);
    for (int i = nargs; i > 0; i -= args_per_block) {
        for (int j = 0; j < L3_MIN(i, args_per_block); j++) {
            l3_log->slots[ofs].body.args[j] = va_arg(args, uint64_t);
        }
        // wrapping increment
        if(++ofs >= L3_MAX_SLOTS) ofs -= L3_MAX_SLOTS;
    }
    va_end(args);

    // Write the footer block.
    l3_log->slots[ofs].foot.tid = l3_my_tid;
    l3_log->slots[ofs].foot.loc = loc;
    l3_log->slots[ofs].foot.msg = msg;
    l3_log->slots[ofs].foot.argN = nargs;
}

int
l3_deinit(void)
{
    return munmap(l3_log, sizeof(*l3_log));
}

/**
 * Appendix:
 *
 * Ref: https://stackoverflow.com/questions/65073550/getting-the-address-of-a-section-with-clang-on-osx-ios
 *
 * Ref: https://stackoverflow.com/questions/17669593/how-to-get-a-pointer-to-a-binary-section-in-mac-os-x/22366882#22366882
 *
 * https://stackoverflow.com/questions/16552710/how-do-you-get-the-start-and-end-addresses-of-a-custom-elf-section
 *
 * https://www.scs.stanford.edu/~dm/blog/va-opt.html
 *
 * Some reference code to try out, some day:
 *
#include <mach-o/getsect.h>

char *secstart;
unsigned long secsize;
secstart = getsectdata("__SEGMENT", "__section", &secsize);
 *
 */
