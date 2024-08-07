/**
 * *****************************************************************************
 * \file test.c -> test-main.c
 * \author Greg Law, Aditya P. Gurajada
 * \brief L3: Lightweight Logging Library Unit-test
 * \version 0.1
 * \date 2023-12-24
 *
 * \copyright Copyright (c) 2023
 *
 * Usage: program-name [--unit-tests]   ; Default, run perf- and unit-tests
 * *****************************************************************************
 */
#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "l3.h"

// Useful constants
#define L3_MILLION      ((uint32_t) (1000 * 1000))
#define L3_NS_IN_SEC    ((uint32_t) (1000 * 1000 * 1000))

void test_perf_slow_logging(int nMil);
void test_perf_fast_logging(int nMil);

// Convert timespec value to nanoseconds units.
static uint64_t inline
timespec_to_ns(struct timespec *ts)
{
    return ((ts->tv_sec * L3_NS_IN_SEC) + ts->tv_nsec);
}

/**
 * The sequence of l3_logging-APIs is flipped in this version of the sample
 * program, as opposed to the .cpp and .cc versions of this exerciser program.
 * This is done so that we can see the different 'arg1' values stashed away
 * by the slow-logging() call, when unpacked by the l3_dump.py script.
 */
int
main(const int argc, const char * argv[])
{
    const char *logfile = NULL;
    if (argc == 1) {

        // Logging perf benchmarking methods below share this log-file.
        logfile = "/tmp/l3.c-test.dat";
        int e = l3_init(logfile);
        if (e) {
            abort();
        }

        int nMil = 300;
        printf("\nExercise in-memory logging performance benchmarking: "
               "%d Mil simple/fast log msgs."
               " L3-log file: %s\n",
                nMil, logfile);

        test_perf_fast_logging(nMil);
        test_perf_slow_logging(nMil);
    }

    if (   (argc == 1)
        || strncmp(argv[1], "--unit-tests", strlen("--unit-tests")) == 0) {
        // Logging unit-tests share this log-file.
        logfile = "/tmp/l3.c-small-test.dat";
        int e = l3_init(logfile);
        if (e) {
            abort();
        }
        printf("L3-logging unit-tests log file: %s\n", logfile);
        l3_log("Simple-log-msg-Args(arg1=%d, arg2=%d)", 1, 2);

        char *bp = (char *) 0xdeadbabe;
        l3_log("Potential memory overwrite (addr=%p, size=%d)", bp, 1024);

        bp = (char *) 0xbeefabcd;
        l3_log("Invalid buffer handle (addr=%p), refcount=%d", bp, 0);

        l3_log_fast("Fast-logging msg1=%d, addr=%p", 10, (void *) 0xdeadbeef);
        l3_log_fast("Fast-logging msg2=%d, addr=%p", 20, (char *) 0xbeefbabe);
    }

    return 0;
}

void
test_perf_slow_logging(int nMil)
{
    struct timespec ts0;
    struct timespec ts1;
    if (clock_gettime(CLOCK_REALTIME, &ts0)) {
        abort();
    }

    uint32_t n = 0;
    for (n = 0; n < (nMil * L3_MILLION); n++) {
        l3_log("Perf-300-Mil Simple l3-log msgs, ctr=%d, value=%d", n, 0);
    }

    if (clock_gettime(CLOCK_REALTIME, &ts1)) {
        abort();
    }
    uint64_t nsec0 = timespec_to_ns(&ts0);
    uint64_t nsec1 = timespec_to_ns(&ts1);

    printf("%d Mil simple log msgs: %" PRIu64 "ns/msg (avg)\n",
            nMil, (nsec1 - nsec0) / n);
}

void
test_perf_fast_logging(int nMil)
{
    struct timespec ts0;
    struct timespec ts1;
    if (clock_gettime(CLOCK_REALTIME, &ts0)) {
        abort();
    }
    uint32_t n = 0;
    for (n = 0; n < (nMil * L3_MILLION); n++) {
        l3_log_fast("Perf-300-Mil Fast l3-log msgs, ctr=%d, value=%d", n, 0);
    }

    if (clock_gettime(CLOCK_REALTIME, &ts1)) {
        abort();
    }
    uint64_t nsec0 = timespec_to_ns(&ts0);
    uint64_t nsec1 = timespec_to_ns(&ts1);

    printf("%d Mil fast log msgs  : %" PRIu64 "ns/msg (avg)\n",
            nMil, (nsec1 - nsec0) / n);
}
