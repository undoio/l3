/**
 * *****************************************************************************
 * \file perf/mt_ubench_fprintf.cpp
 * \author Greg Law
 * \brief fprintf micro perf-benchmarking
 * \version 0.1
 * \date 2025-02-21
 *
 * \copyright Copyright (c) 2025
 *
 * Usage: program-name [ <number-of-threads> ]
 * *****************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <pthread.h>

// Micro perf benchmarking workload parameter defaults
constexpr int L3_PERF_UBM_NTHREADS = 10;
constexpr int L3_PERF_UBM_NMSGS = (1000 * 1000);

#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
pthread_barrier_t barrier;
#endif

int main(int argc, char** argv) {
    int nthreads = ((argc > 1) ? atoi(argv[1]) : L3_PERF_UBM_NTHREADS);
    // int nmsgs = ((argc > 2) ? atoi(argv[2]) : L3_PERF_UBM_NMSGS);

    FILE *f = fopen("/tmp/fprintf.log", "w");
    assert(f);

    pthread_t threads[nthreads];
#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
    int e = pthread_barrier_init(&barrier, NULL, nthreads+1);
    assert(!e);
#endif

    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, [](void *p) -> void * {
#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
            int e = pthread_barrier_wait(&barrier);
            assert(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD);
#endif
            FILE *f = (FILE *)p;
            for (int j = 0; j < L3_PERF_UBM_NMSGS; j++) {
                fprintf(f, "Hello, world! %d %d", 0, j);
            }
            return nullptr;
        }, f);
    }
#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
    pthread_barrier_wait(&barrier);
    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);
#endif

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    gettimeofday(&tv1, NULL);
    long us = (tv1.tv_sec * 1000000 + tv1.tv_usec) - (tv0.tv_sec * 1000000 + tv0.tv_usec);
    printf("%d,%02.2f\n", nthreads, (double)us / L3_PERF_UBM_NMSGS);
    fclose(f);
    return 0;
}
