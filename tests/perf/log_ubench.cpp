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

#include "../../l3.h"

pthread_barrier_t barrier;

struct timeval tv0, tv1;
int nthreads, completed, started;
long nmsgs = (1024 * 1024);

size_t *idx_ptr;
const size_t max_msg_len = 128;
const size_t buff_size = max_msg_len * L3_MAX_SLOTS;
const size_t buff_mask = buff_size - 1;

std::shared_ptr<spdlog::logger> logger;

void*
time_fprintf(void * arg)
{
    int tid = syscall(SYS_gettid);
    FILE *f = (FILE *)arg;
    int e = pthread_barrier_wait(&barrier);
    assert(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD);
    if (__sync_fetch_and_add(&started, 1) == 0) gettimeofday(&tv0, NULL);

    for (int j = 0; j < nmsgs; j++) {
        fprintf(f, "%d: Hello, world! Here is argument one %d and argument two is %p\n", tid, j, arg);
    }

    if (__sync_fetch_and_add(&completed, 1) == nthreads - 1) gettimeofday(&tv1, NULL);
    return nullptr;
}

void*
time_spdlog(void * arg)
{
    int tid = syscall(SYS_gettid);
    int e = pthread_barrier_wait(&barrier);
    assert(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD);
    if (__sync_fetch_and_add(&started, 1) == 0) gettimeofday(&tv0, NULL);

    for (int j = 0; j < nmsgs; j++) {
        logger->info("{}: Hello, world! Here is argument one {} and argument two is {}\n", tid, j, arg);
    }

    if (__sync_fetch_and_add(&completed, 1) == nthreads - 1) gettimeofday(&tv1, NULL);
    return nullptr;
}

void
do_sprintf(char *buffer, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    /* First compute the size we'll need, by doing vsnprintf into a zero-size buffer. */
    size_t len = vsnprintf(buffer, 0, fmt, args);

    /* Then reserve that many bytes so that we may log into it. */
    uintptr_t i = __sync_fetch_and_add(idx_ptr, len-1) & buff_mask;
    vsnprintf(((char*)buffer) + i, buff_size - i, fmt, args);

    if (len > buff_size - i) {
        /* If we wrapped around, we need to log the rest of the message at the beginning of the buffer. */
        vsnprintf(buffer, i, fmt + len - (buff_size-i), args);
    }
    va_end(args);
}

void*
time_sprintf(void *arg)
{
    int tid = syscall(SYS_gettid);
    int e = pthread_barrier_wait(&barrier);
    assert(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD);
    if (__sync_fetch_and_add(&started, 1) == 0) gettimeofday(&tv0, NULL);

    for (int j = 0; j < nmsgs; j++)
    {
        do_sprintf((char*) arg, "%d: Hello, world! Here is argument one %d and argument two is %p\n", tid, j, arg);
    }

    if (__sync_fetch_and_add(&completed, 1) == nthreads - 1) gettimeofday(&tv1, NULL);
    return nullptr;
}

void*
time_l3(void *arg)
{
    int e = pthread_barrier_wait(&barrier);
    assert(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD);
    if (__sync_fetch_and_add(&started, 1) == 0) gettimeofday(&tv0, NULL);

    for (int j = 0; j < nmsgs; j++) {
        l3_log("Hello, world! Here is argument one %d and argument two is %d", j, nmsgs);
    }

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

    void *(*fn)(void*);
    void *arg;
    if (!strcmp(argv[1], "fprintf"))
    {
        fn = time_fprintf;
        arg = fopen("/tmp/log", "w+");
        assert(arg);
    }
    else if(!strcmp(argv[1], "spdlog"))
    {
        logger = spdlog::basic_logger_mt("file_logger", "/tmp/logger.txt", true);
        logger->enable_backtrace(buff_size);
        fn = time_spdlog;
    }
    else if (!strcmp(argv[1], "sprintf"))
    {
        nmsgs *= 8;
        int fd = open("/tmp/sprintf.log", O_RDWR | O_CREAT, 0666);
        assert( fd>=0);
        char page[sysconf(_SC_PAGE_SIZE)];
        for (int i = 0; i < buff_size + sysconf(_SC_PAGE_SIZE); i+=sizeof(page)) {
            int r = write(fd, page, sizeof(page));
            assert(r== sizeof(page));
        }
        arg = mmap(NULL, buff_size + sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        assert(arg != MAP_FAILED);
        idx_ptr = (size_t*)(((char*)arg) + buff_size);
        fn = time_sprintf;
    }
    else if (!strcmp(argv[1], "l3"))
    {
        nmsgs *= 8;
        l3_init("/tmp/l3.log");
        fn = time_l3;
    }
    else
        usage(argv[0]);

    nthreads = ((argc > 2) ? atoi(argv[2]) : 10);

    pthread_t threads[nthreads];
    int e = pthread_barrier_init(&barrier, NULL, nthreads);
    assert(!e);
    
    for (int i = 0; i < nthreads; i++)
    {
        pthread_create(&threads[i], NULL, fn, arg);
    }

    for (int i = 0; i < nthreads; i++)
    {
        pthread_join(threads[i], NULL);
    }
    long us = (tv1.tv_sec * 1000000 + tv1.tv_usec) - (tv0.tv_sec * 1000000 + tv0.tv_usec);
    printf("%ld\n", us * 1000 / nmsgs);
    return 0;
}
