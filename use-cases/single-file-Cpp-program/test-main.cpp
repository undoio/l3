/**
 * *****************************************************************************
 * \file test-main.cpp
 * \author Aditya P. Gurajada, Greg Law
 * \brief L3: Lightweight Logging Library C++ Unit-test
 * \version 0.1
 * \date 2023-12-24
 *
 * \copyright Copyright (c) 2024
 * *****************************************************************************
 */
#define _POSIX_C_SOURCE 199309L

#include <iostream>

#include "l3.h"

using namespace std;

// Useful constants
constexpr auto L3_MILLION      (1000 * 1000);
constexpr auto L3_NS_IN_SEC    (1000 * 1000 * 1000);

void test_perf_slow_logging(int nMil);
void test_perf_fast_logging(int nMil);

// Convert timespec value to nanoseconds units.
static uint64_t inline
timespec_to_ns(struct timespec *ts)
{
    return ((ts->tv_sec * L3_NS_IN_SEC) + ts->tv_nsec);
}

int
main(void)
{
    // Logging perf benchmarking methods below share this log-file.
    auto e = l3_init("/tmp/l3.cpp-test.dat");
    if (e) {
        abort();
    }

    auto nMil = 300;
    printf("\nExercise in-memory logging performance benchmarking: "
           "%d Mil simple/fast log msgs\n",
            nMil);

    test_perf_slow_logging(nMil);
    test_perf_fast_logging(nMil);

    // Logging unit-tests share this log-file.
    e = l3_init("/tmp/l3.cpp-small-test.dat");
    if (e) {
        abort();
    }
    l3_log_simple("Simple-log-msg-Args(1,2)", 1, 2);
    l3_log_simple("Potential memory overwrite (addr, size)", 0xdeadbabe, 1024);
    l3_log_simple("Invalid buffer handle (addr)", 0xbeefabcd, 0);
    l3_log_fast("Fast-logging msg1", 10, 0xdeadbeef);
    l3_log_fast("Fast-logging msg2", 20, 0xbeefbabe);

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

    auto n = 0;
    for (; n < (nMil * L3_MILLION); n++) {
        l3_log_simple("300-Mil Simple l3-log msgs", 0, 0);
    }

    if (clock_gettime(CLOCK_REALTIME, &ts1)) {
        abort();
    }
    auto nsec0 = timespec_to_ns(&ts0);
    auto nsec1 = timespec_to_ns(&ts1);

    cout << nMil << " Mil simple log msgs: " << ((nsec1 - nsec0) / n)
         << "ns/msg (avg)" << endl;
}

void
test_perf_fast_logging(int nMil)
{
    struct timespec ts0;
    struct timespec ts1;
    if (clock_gettime(CLOCK_REALTIME, &ts0)) {
        abort();
    }

    // Throw-in some variations to generate diff 'arg' values during logging.
    auto n = 0;
    for (; n < (300 * L3_MILLION); n++) {
        l3_log_fast("300-Mil Fast l3-log msgs", (n/L3_MILLION), n);
    }

    if (clock_gettime(CLOCK_REALTIME, &ts1)) {
        abort();
    }
    auto nsec0 = timespec_to_ns(&ts0);
    auto nsec1 = timespec_to_ns(&ts1);

    cout << nMil << " Mil fast log msgs: " << ((nsec1 - nsec0) / n)
         << "ns/msg (avg)" << endl;
}
