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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/syscall.h>
#include <pthread.h>

#include "l3.h"

// Micro perf benchmarking workload parameter defaults
constexpr int L3_PERF_UBM_NTHREADS = 10;
constexpr int L3_PERF_UBM_NMSGS = (1000 * 1000);

#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
pthread_barrier_t barrier;
#endif

int main(int argc, char** argv) {
    int nthreads = ((argc > 1) ? atoi(argv[1]) : L3_PERF_UBM_NTHREADS);
    // int nmsgs = ((argc > 2) ? atoi(argv[2]) : L3_PERF_UBM_NMSGS);

    pthread_t threads[nthreads];
#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
    int e = pthread_barrier_init(&barrier, NULL, nthreads+1);
    assert(!e);
#endif

    l3_init("/tmp/l3.log");
    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, [](void *) -> void * {
#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
            int e = pthread_barrier_wait(&barrier);
            assert(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD);
#endif
            for (int j = 0; j < L3_PERF_UBM_NMSGS; j++) {
                l3_log("Hello, world! %d %d", 0, j);
            }
            return nullptr;
        }, nullptr);
    }
#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
    pthread_barrier_wait(&barrier);
#endif

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}
