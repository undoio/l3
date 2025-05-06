/**
 * *****************************************************************************
 * \file perf/mt_ubench_l3.cpp
 * \author Greg Law
 * \brief L3: Fast-logging micro perf-benchmarking
 * \version 0.1
 * \date 2024-06-25
 *
 * \copyright Copyright (c) 2024
 *
 * Usage: program-name [ <number-of-threads> ]
 * *****************************************************************************
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <sys/mman.h>

#include <fstream>

#include "../../l3.h"

static pthread_barrier_t barrier;
static struct timeval tv0, tv1;
static int nthreads, completed, started;
static const long nmsgs = 1024 * 1024;
static const size_t max_msg_len = 128;
static const size_t buff_size = max_msg_len * L3_MAX_SLOTS;

static void
many_fprintf(void)
{
    int tid = syscall(SYS_gettid);

    FILE *f = fopen("/tmp/log", "w+");
    assert(f);

    for (int j = 0; j < nmsgs; j++)
    {
        fprintf(f, "%d: Hello, world! Here is argument one %d and argument two is %p\n", tid, j, &j);
    }

    if (__sync_fetch_and_add(&completed, 1) == nthreads - 1) gettimeofday(&tv1, NULL);
}

static void
many_stream(void)
{
    int tid = syscall(SYS_gettid);
    std::ofstream fout("/tmp/stream");
    for (int j = 0; j < nmsgs; j++)
    {
        fout << tid << ": Hello, world! Here is argument one " << j << " and argument two is " << &j << '\n';
    }
}

std::shared_ptr<spdlog::logger> logger;
static void
many_spdlog(void)
{
    logger = spdlog::basic_logger_mt("file_logger", "/tmp/logger.txt", true);
    logger->enable_backtrace(buff_size);
    assert(logger);
    int tid = syscall(SYS_gettid);
    for (int j = 0; j < nmsgs; j++)
    {
        logger->info("%d: Hello, world! Here is argument one %d and argument two is %p\n", tid, j, (void*) &j);
    }
}

static void
do_sprintf(char *buffer, size_t *idx_ptr, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    /* First compute the size we'll need, by doing vsnprintf into a zero-size buffer. */
    size_t len = vsnprintf(buffer, 0, fmt, args);

    /* Then reserve that many bytes so that we may log into it. */
    uintptr_t i = __sync_fetch_and_add(idx_ptr, len) % buff_size;
    va_start(args, fmt);
    vsnprintf(((char*)buffer) + i, buff_size - i, fmt, args);

    if (len > buff_size - i) {
        /* If we wrapped around, we need to log the rest of the message at the beginning of the buffer. */
        vsnprintf(buffer, i, fmt + len - (buff_size-i), args);
    }
    va_end(args);
}

static void
many_sprintf(void)
{
    int fd = open("/tmp/sprintf.log", O_RDWR | O_CREAT, 0666);
    assert( fd>=0);
    int r = ftruncate(fd, buff_size + sysconf(_SC_PAGE_SIZE));
    assert(r == 0);
    char *buffer = (char *) mmap(NULL, buff_size + sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(buffer != MAP_FAILED);
    madvise(buffer, buff_size + sysconf(_SC_PAGE_SIZE), MADV_POPULATE_WRITE);
    size_t *idx_ptr = (size_t*)(((char*)buffer) + buff_size);
    int tid = syscall(SYS_gettid);
    for (long j = 0; j < nmsgs; j++)
    {
        do_sprintf(buffer, idx_ptr, "%d: Hello, world! Here is argument one %ld and argument two is %p\n", tid, j, &j);
    }
}

static void
many_l3(void)
{
    l3_init("/tmp/l3.log");
    for (int j = 0; j < nmsgs; j++)
    {
        l3_log("Hello, world! Here is argument one %d and argument two is %p", j, &j);
    }
}

void*
go(void *arg)
{
    void (*fn)(void) = (void (*)(void)) arg;
    int tid = syscall(SYS_gettid);
    int e = pthread_barrier_wait(&barrier);
    assert(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD);
    if (__sync_fetch_and_add(&started, 1) == 0) gettimeofday(&tv0, NULL);

    fn();

    if (__sync_fetch_and_add(&completed, 1) == nthreads - 1) gettimeofday(&tv1, NULL);
    return nullptr;
}


static void
usage(const char *progname)
{
    fprintf(stderr, "Usage: %s <mode> [ <number-of-threads> ]\n", progname);
    exit(1);
}

int
main(int argc, char** argv)
{
    if (argc < 2) usage(argv[0]);

    void (*fn)(void);
    if (!strcmp(argv[1], "fprintf"))
    {
        fn = many_fprintf;
    }
    else if (!strcmp(argv[1], "stream"))
    {
        fn = many_stream;
    }
    else if(!strcmp(argv[1], "spdlog"))
    {
        fn = many_spdlog;
    }
    else if (!strcmp(argv[1], "sprintf"))
    {
        fn = many_sprintf;
    }
    else if (!strcmp(argv[1], "l3"))
    {
        fn = many_l3;
    }
    else
        usage(argv[0]);

    nthreads = ((argc > 2) ? atoi(argv[2]) : 10);

    int e = pthread_barrier_init(&barrier, NULL, nthreads);
    assert(!e);

    if (nthreads == 1)
    {
        go((void *)fn);
    }
    else
    {
        pthread_t threads[nthreads];

        for (int i = 0; i < nthreads; i++)
        {
            pthread_create(&threads[i], NULL, go, (void *) fn);
        }

        for (int i = 0; i < nthreads; i++)
        {
            pthread_join(threads[i], NULL);
        }
    }
    long us = (tv1.tv_sec * 1000000 + tv1.tv_usec) - (tv0.tv_sec * 1000000 + tv0.tv_usec);
    printf("%ld\n", us * 1000 / nmsgs);
    return 0;
}
