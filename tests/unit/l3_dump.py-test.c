/**
 * *****************************************************************************
 * \file l3_dump.py-test.c
 *
 * NOTE: This stand-alone unit-test exercises logic in the Python script,
 * `l3_dump.py`, to verify that more than a hard-coded few log-entries can
 * be successfully unpacked. Makefile rules currently do not support running
 * unit-tests with the `L3_LOC_ENABLED=1` env-var.
 *
 * Do -NOT- do that while running tests; things will fail awkwardly.
 *
 * \author Aditya P. Gurajada
 * \brief L3: Lightweight Logging Library - Exerciser Unit-test
 * \version 0.1
 * \date 2024-03-27
 *
 * \copyright Copyright (c) 2024
 * *****************************************************************************
 */
#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdio.h>

#include "l3.h"

// Function prototypes
void test_l3_slow_log(void);
void test_l3_fast_log(void);

int
main(const int argc, const char **argv)
{
    test_l3_fast_log();
    test_l3_slow_log();

    return 0;
}

void test_l3_slow_log(void)
{
    const char *log = "/tmp/l3.c-small-unit-test.dat";
    int e = l3_init(log);
    if (e) {
        abort();
    }
    l3_log_simple("Simple-log-msg-Args(1,2)", 1, 2);
    l3_log_simple("Simple-log-msg-Args(3,4)", 3, 4);
    l3_log_simple("Potential memory overwrite (addr, size)", 0xdeadbabe, 1024);
    l3_log_simple("Invalid buffer handle (addr)", 0xbeefabcd, 0);

    printf("Generated slow log-entries to log-file: %s\n", log);
}

void test_l3_fast_log(void)
{
    const char *log = "/tmp/l3.c-fast-unit-test.dat";
    int e = l3_init(log);
    if (e) {
        abort();
    }
    l3_log_fast("Fast-log-msg: Args(1,2)");
    l3_log_fast("Fast-log-msg: Args(3,4)");
    l3_log_fast("Fast-log-msg: Potential memory overwrite (addr, size)");
    l3_log_fast("Fast-log-msg: Invalid buffer handle (addr)");

    printf("Generated fast log-entries to log-file: %s\n", log);
}
