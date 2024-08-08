/**
 * *****************************************************************************
 * \file l3-fprintf-perf-test.c
 * \author Aditya P. Gurajada
 * \brief L3: Lightweight Logging Library fprintf() logging APIs Unit-test
 * \version 0.1
 * \date 2024-05-29
 *
 * \copyright Copyright (c) 2024
 *
 * Usage: program-name [ <number-of-million-msgs> ]
 *  Default, run fprintf()-logging perf-tests on 1 million messages
 * *****************************************************************************
 */
#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "tests/l3_test.h"
#include "l3-perf-test.h"

void test_fprintf_logging_perf(int nMil);

/**
 * *****************************************************************************
 * The L3 logging-APIs to exercise logging using fprintf() system call are
 * exercised in this unit-test. As a convenience, we just log a large # of
 * entries to get a rough measure of logging throughput using this interface.
 * *****************************************************************************
 */
int
main(const int argc, const char * argv[])
{
    int nMil = 1;
    if (argc > 1) {
        nMil = atoi(argv[1]);
    }

    test_fprintf_logging_perf(nMil);

    return 0;
}

#if L3_LOGT_FPRINTF

void
test_fprintf_logging_perf(int nMil)
{
    // Logging perf benchmarking methods below share this log-file.
    const char *logfile = "/tmp/l3-fprintf-logging-test.dat";
    int e = l3_test_log_init(L3_LOG_FPRINTF, logfile);
    if (e) {
        abort();
    }
    test_logging_perf("fprintf", nMil, logfile);
}

#endif  // L3_LOGT_FPRINTF

/**
 * *****************************************************************************
 * test_logging_perf() - Common function to log n-msgs and measure perf metrics.
 * NOTE: This cannot be a shared function in common .c file as the build-time
 * -D<flag> dictates which version of l3_log() [ write / fprintf ] will be used.
 * *****************************************************************************
 */
void
test_logging_perf(const char *logtype, int nMil, const char *filename)
{
    struct timespec ts0;
    struct timespec ts1;
    if (clock_gettime(CLOCK_REALTIME, &ts0)) {
        abort();
    }

    uint32_t n = 0;
    for (n = 0; n < (nMil * L3_MILLION); n++) {
        l3_log("Perf-l3-log msgs, ctr=%d, value=%d\n", n, 0);
    }

    if (clock_gettime(CLOCK_REALTIME, &ts1)) {
        abort();
    }
    uint64_t nsec0 = timespec_to_ns(&ts0);
    uint64_t nsec1 = timespec_to_ns(&ts1);

    printf("%d Mil %s() log msgs: %" PRIu64 " ns/msg (avg): %s\n",
            nMil, logtype, ((nsec1 - nsec0) / n), filename);
}
