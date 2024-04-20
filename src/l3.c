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

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <stdio.h>

#if __APPLE__
#include <mach-o/getsect.h>
#include <pthread.h>
#else
#include <threads.h>
#endif  // __APPLE__

#include <string.h>
#include <assert.h>

#include "l3.h"

#ifdef L3_LOC_ENABLED
#include "loc.h"
#endif  // L3_LOC_ENABLED

// Define version of static assertion checking that works for both gcc and g++
#ifdef __cplusplus
#define L3_STATIC_ASSERT    static_assert
#else
#define L3_STATIC_ASSERT    _Static_assert
#endif

#if __APPLE__
#define L3_THREAD_LOCAL
#define L3_GET_TID()  0
#else
#define L3_THREAD_LOCAL thread_local
#define L3_GET_TID()  syscall(SYS_gettid)
#endif  // __APPLE__

/**
 * L3 Log entry Structure definitions:
 */
typedef struct l3_entry
{
    pid_t       tid;
#ifdef L3_LOC_ENABLED
    loc_t       loc;
#else
    uint32_t    loc;
#endif  // L3_LOC_ENABLED
    const char *msg;
    uint64_t    arg1;
    uint64_t    arg2;
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
 * L3 Log Structure definitions:
 */
typedef struct l3_log
{
    uint64_t idx;
    uint64_t fbase_addr;
    uint64_t pad0;
    uint64_t pad1;
    L3_ENTRY slots[L3_MAX_SLOTS];
} L3_LOG;

L3_LOG *l3_log; // Also referenced in l3.S for fast-logging.

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

// ****************************************************************************
int
l3_init(const char *path)
{
    int fd = -1;
    if (path)
    {
        fd = open(path, O_RDWR | O_CREAT, 0666);
        if (fd == -1) {
            return -1;
        }

        if (lseek(fd, sizeof(L3_LOG), SEEK_SET) < 0) {
            return -1;
        }
        if (write(fd, &fd, 1) != 1) {
            return -1;
        }
    }

    l3_log = (L3_LOG *) mmap(NULL, sizeof(*l3_log), PROT_READ|PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (l3_log == MAP_FAILED) {
        return -1;
    }

    // Technically, this is not needed as mmap() is guaranteed to return
    // zero-filled pages. We do this just to be clear where the idx begins.
    l3_log->idx = 0;

#if __APPLE__
     l3_log->fbase_addr= getBaseAddress();
#else

    /* Linux: Let's find where rodata is loaded. */
    Dl_info info;
    if (!dladdr("test string", &info))
    {
        errno = L3_DLADDR_NOT_FOUND_ERRNO;
        return -1;
    }
    l3_log->fbase_addr = (intptr_t) info.dli_fbase;
#endif  // __APPLE__

    // printf("fbase_addr=%" PRIu64 " (0x%llx)\n", l3_log->fbase_addr, l3_log->fbase_addr);

    return 0;
}

// ****************************************************************************
static pid_t
l3_mytid(void)
{
    static L3_THREAD_LOCAL pid_t tid;
    if (!tid) {
        tid = L3_GET_TID();
    }

    return tid;
}

// ****************************************************************************
void
#ifdef L3_LOC_ENABLED
l3__log_simple(loc_t loc, const char *msg, const uint64_t arg1, const uint64_t arg2)
#else
l3__log_simple(uint32_t loc, const char *msg, const uint64_t arg1, const uint64_t arg2)
#endif
{
    int idx = __sync_fetch_and_add(&l3_log->idx, 1) % L3_MAX_SLOTS;
    l3_log->slots[idx].tid = l3_mytid();

#ifdef L3_LOC_ENABLED
    l3_log->slots[idx].loc = (loc_t) loc;
#else
#ifdef DEBUG
    assert(l3_log->slots[idx].loc == 0);
#endif  // DEBUG
#endif

    l3_log->slots[idx].msg = msg;
    l3_log->slots[idx].arg1 = arg1;
    l3_log->slots[idx].arg2 = arg2;
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
 * Some reference code to try out, some day:
 *
#include <mach-o/getsect.h>

char *secstart;
unsigned long secsize;
secstart = getsectdata("__SEGMENT", "__section", &secsize);
 *
 */
