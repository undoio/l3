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

int
main(const int argc, const char **argv)
{
    const char *log = "/tmp/l3.c-unit-test.dat";
    int e = l3_init(log);
    if (e) {
        abort();
    }
    l3_log("log-msg: Args(arg1=%d, arg2=%d)", 1, 2);
    l3_log("log-msg: Args(arg3=%d, arg4=%d)", 3, 4);
    l3_log("log-msg: Args(arg1=%d, arg2=%d)", 10, 20);
    l3_log("log-msg: Potential memory overwrite (addr=0x%x, size=%d)", 0xdeadbabe, 1024);

    void *bp = (void *) 0xbeefabcd;
    l3_log("log-msg: Invalid buffer handle (addr=0x%p), unused=%u", bp, 0);

    printf("Generated log-entries to log-file: %s\n", log);
}
