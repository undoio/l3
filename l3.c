#define _GNU_SOURCE
#include <dlfcn.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include "l3.h"

struct l3_entry
{
    pid_t       tid;
    const char *msg;
    uint64_t    arg1;
    uint64_t    arg2;
};

struct l3_log
{
    uint64_t idx;
    uint64_t fbase_addr;
    uint64_t pad0;
    uint64_t pad1;
    struct l3_entry slots[L3_MAX_SLOTS];
} *l3_log;

int
l3_init(const char *path)
{
    int fd = -1;
    if (path)
    {
        fd = open(path, O_RDWR | O_CREAT, 0666);
        if (fd == -1) return -1;

        if (lseek(fd, sizeof(struct l3_log), SEEK_SET) < 0) return -1;
        if (write(fd, &fd, 1) != 1) return -1;
    }

    l3_log = mmap(NULL, sizeof(*l3_log), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (l3_log == MAP_FAILED) return -1;

    l3_log->idx = 0;

    /* Let's find where rodata is loaded. */
    Dl_info info;
    if (!dladdr("test string", &info))
    {
        errno = 1234;
        return -1;
    }
    l3_log->fbase_addr = (intptr_t) info.dli_fbase;
    return 0;
}

pid_t
mytid(void)
{
    static _Thread_local pid_t tid;
    if (!tid) tid = syscall(SYS_gettid);
    return tid;
}

void
l3_log_simple(const char *msg, uint64_t arg1, uint64_t arg2)
{
    int idx = __sync_fetch_and_add(&l3_log->idx, 1) % L3_MAX_SLOTS;
    l3_log->slots[idx].tid = mytid();
    l3_log->slots[idx].msg = msg;
    l3_log->slots[idx].arg1 = arg1;
    l3_log->slots[idx].arg2 = arg2;
}
