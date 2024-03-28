/**
 * *****************************************************************************
 * \file l3_dump.py-test.c
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
main()
{
    int e = l3_init("/tmp/l3.c-small-test.dat");
    if (e) {
        abort();
    }
    l3_log_simple("Simple-log-msg-Args(1,2)", 1, 2);
    l3_log_simple("Potential memory overwrite (addr, size)", 0xdeadbabe, 1024);
    l3_log_simple("Invalid buffer handle (addr)", 0xbeefabcd, 0);

    return 0;
}

