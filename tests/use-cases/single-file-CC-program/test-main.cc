/**
 * *****************************************************************************
 * \file test-main.cc
 * \author Aditya P. Gurajada, Greg Law
 * \brief L3: Lightweight Logging Library C++ Unit-test
 * \version 0.1
 * \date 2023-12-24
 *
 * \copyright Copyright (c) 2024
 *
 * Usage: program-name [--unit-tests]   ; Default, run perf- and unit-tests
 * *****************************************************************************
 */
#define _POSIX_C_SOURCE 199309L

#include <iostream>

#include <string.h>
#include "l3.h"

using namespace std;

// Useful constants
constexpr auto L3_MILLION      (1000 * 1000);
constexpr auto L3_NS_IN_SEC    (1000 * 1000 * 1000);

void test_perf_logging(int nMil);

// Convert timespec value to nanoseconds units.
static uint64_t inline
timespec_to_ns(struct timespec *ts)
{
    return ((ts->tv_sec * L3_NS_IN_SEC) + ts->tv_nsec);
}

int
main(const int argc, const char * argv[])
{
    const char *logfile = NULL;
    if (argc == 1) {
        // Logging perf benchmarking methods below share this log-file.
        logfile = "/tmp/l3.cc-test.dat";
        auto e = l3_init(logfile);
        if (e) {
            abort();
        }

        auto nMil = 300;
        cout << "\nExercise in-memory logging performance benchmarking: "
             << nMil << " Mil log msgs."
             << " L3-log file: " << logfile << "\n";

        test_perf_logging(nMil);
    }

    if (   (argc == 1)
        || strncmp(argv[1], "--unit-tests", strlen("--unit-tests")) == 0) {
        // Logging unit-tests share this log-file.
        logfile = "/tmp/l3.cc-small-test.dat";
        int e = l3_init(logfile);
        if (e) {
            abort();
        }
        cout << "L3-logging unit-tests log file: " << logfile << "\n";
        l3_log("Log-msg-Args(arg1=%d, arg2=%d)", 1, 2);

        int *lockp = (int *) 0xdeadbabe;
        l3_log("Potential memory overwrite (addr=%p, size=%u)", lockp, 1024);

        lockp = (int *) 0xbeefabcd;
        l3_log("Invalid buffer handle (addr=%p, refcount=%d)", lockp, 0);
    }

    return 0;
}

void
test_perf_logging(int nMil)
{
    struct timespec ts0;
    struct timespec ts1;
    if (clock_gettime(CLOCK_REALTIME, &ts0)) {
        abort();
    }

    auto n = 0;
    for (; n < (nMil * L3_MILLION); n++) {
        l3_log("Perf-300-Mil l3-log msgs, i=%d, j=%d", 0, 0);
    }

    if (clock_gettime(CLOCK_REALTIME, &ts1)) {
        abort();
    }
    auto nsec0 = timespec_to_ns(&ts0);
    auto nsec1 = timespec_to_ns(&ts1);

    cout << nMil << " Mil log msgs: " << ((nsec1 - nsec0) / n)
         << "ns/msg (avg)" << endl;
}
