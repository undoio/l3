/**
 * *****************************************************************************
 * \file perf/mt_ubench_spdlog.cpp
 * \author Greg Law
 * \brief L3: spdlog-multi-threaded logging micro perf-benchmarking
 * \version 0.1
 * \date 2024-06-25
 *
 * \copyright Copyright (c) 2024
 *
 * Usage: program-name [ <number-of-threads> ]
 * *****************************************************************************
 */
#include <stdlib.h>
#include <pthread.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>


// Micro perf benchmarking workload parameter defaults
constexpr int L3_PERF_UBM_NTHREADS = 10;
constexpr int L3_PERF_UBM_NMSGS = (1000 * 1000);
constexpr int L3_PERF_UBM_LOG_SIZE = (16 * 1024);

#if __APPLE__
#define L3_GET_TID()    pthread_mach_thread_np(pthread_self())
#else
#define L3_GET_TID()    syscall(SYS_gettid)
#endif  // __APPLE__

std::shared_ptr<spdlog::logger> logger;

#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
pthread_barrier_t barrier;
#endif

int main(int argc, char** argv) {
    int nthreads = ((argc > 1) ? atoi(argv[1]) : L3_PERF_UBM_NTHREADS);
    // int nmsgs = ((argc > 2) ? atoi(argv[2]) : L3_PERF_UBM_NMSGS);

    pthread_t threads[nthreads];

#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
    pthread_barrier_init(&barrier, NULL, (nthreads + 1));
#endif

    logger = spdlog::basic_logger_mt("file_logger", "/tmp/logger.txt", true);
    logger->enable_backtrace(L3_PERF_UBM_LOG_SIZE);

    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, [](void*) -> void* {
            int tid = L3_GET_TID();
#if !defined(_POSIX_BARRIERS) || _POSIX_BARRIERS > 0
            pthread_barrier_wait(&barrier);
#endif
            for (int j = 0; j < L3_PERF_UBM_NMSGS; j++) {
                logger->info("{}: Hello, World! tid: {}", j, tid);
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
