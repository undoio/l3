/**
 * *****************************************************************************
 * \file test.c -> test-main.c
 * \author Greg Law
 * \brief L3: Lightweight Logging Library Unit-test
 * \version 0.1
 * \date 2023-12-24
 *
 * \copyright Copyright (c) 2023
 * *****************************************************************************
 */
#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdio.h>

#include "l3.h"

// Useful constants
#define L3_MILLION      ((uint32_t) (1000 * 1000))
#define L3_NS_IN_SEC    ((uint32_t) (1000 * 1000 * 1000))

// Convert timespec value to nanoseconds units.
static uint64_t inline
timespec_to_ns(struct timespec *ts)
{
    return ((ts->tv_sec * L3_NS_IN_SEC) + ts->tv_nsec);
}

/**
 * The sequence of l3_log_fast() and l3_log_simple() is flipped in this version
 * as opposed to the .cpp and .cc versions of this test exerciser program.
 * This is done so that we can see the different 'arg1' values stashed away
 * by the l3_log_simple() call, when unpacked by the l3_dump.py script.
 */
int
main(void)
{
    int e = l3_init("/tmp/l3.c-test.dat");
    if (e) {
        abort();
    }

    struct timespec ts0;
    struct timespec ts1;
    if (clock_gettime(CLOCK_REALTIME, &ts0)) {
        abort();
    }

    int nMil = 300;
    printf("\nExercise in-memory logging performance benchmarking: "
           "%d Mil simple/fast log msgs\n",
            nMil);
    unsigned long n;

    if (clock_gettime(CLOCK_REALTIME, &ts0)) {
        abort();
    }

    for (n = 0; n < (300 * L3_MILLION); n++) {
        l3_log_fast("300-Mil Fast l3-log msgs", n, 0);
    }

    if (clock_gettime(CLOCK_REALTIME, &ts1)) {
        abort();
    }
    uint64_t nsec0 = timespec_to_ns(&ts0);
    uint64_t nsec1 = timespec_to_ns(&ts1);

    printf("%d Mil fast log msgs  : %" PRIu64 "ns/msg (avg)\n",
            nMil, (nsec1 - nsec0) / n);

    for (n = 0; n < (nMil * L3_MILLION); n++) {
        l3_log_simple("300-Mil Simple l3-log msgs", n, 0);
    }

    if (clock_gettime(CLOCK_REALTIME, &ts1)) {
        abort();
    }
    nsec0 = timespec_to_ns(&ts0);
    nsec1 = timespec_to_ns(&ts1);

    printf("%d Mil simple log msgs: %" PRIu64 "ns/msg (avg)\n",
            nMil, (nsec1 - nsec0) / n);

    e = l3_init("/tmp/l3.c-small-test.dat");
    if (e) {
        abort();
    }
    l3_log_simple("Simple-log-msg-Args(1,2)", 1, 2);
    l3_log_simple("Potential memory overwrite (addr, size)", 0xdeadbabe, 1024);
    l3_log_simple("Invalid buffer handle (addr)", 0xbeefabcd, 0);

    return 0;
}
