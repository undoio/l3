#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdio.h>

#include "l3.h"

int
main(void)
{
    int e = l3_init("/tmp/l3_test");
    if (e) abort();

    struct timespec ts0, ts1;
    if (clock_gettime(CLOCK_REALTIME, &ts0)) abort();

    unsigned long n;
    for (n = 0; n < 300000000; n++)
        l3_log_simple("hello world", 0, 0);

    if (clock_gettime(CLOCK_REALTIME, &ts1)) abort();
    uint64_t nsec0 = ts0.tv_sec * 1000000000 + ts0.tv_nsec;
    uint64_t nsec1 = ts1.tv_sec * 1000000000 + ts1.tv_nsec;

    printf("simple: %" PRIu64 "ns\n", (nsec1 - nsec0) / n);

    if (clock_gettime(CLOCK_REALTIME, &ts0)) abort();

    for (n = 0; n < 300000000; n++)
        l3_log_fast("hello world");

    if (clock_gettime(CLOCK_REALTIME, &ts1)) abort();
    nsec0 = ts0.tv_sec * 1000000000 + ts0.tv_nsec;
    nsec1 = ts1.tv_sec * 1000000000 + ts1.tv_nsec;

    printf("fast: %" PRIu64 "ns\n", (nsec1 - nsec0) / n);

    e = l3_init("/tmp/l3_small_test");
    if (e) abort();
    l3_log_simple("test1", 1, 2);
    l3_log_simple("test2", 2, 3);
    l3_log_simple("test3", 3, 4);

    return 0;
}
