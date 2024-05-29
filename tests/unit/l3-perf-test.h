/**
 * *****************************************************************************
 * \file l3-perf-test.h
 * \author Aditya P. Gurajada
 * \brief L3: Lightweight Logging Library performance unit-test header
 * \version 0.1
 * \date 2024-05-29
 *
 * \copyright Copyright (c) 2024
 * *****************************************************************************
 */
#include <stdio.h>
#include <time.h>
#include "l3.h"

// Useful constants
#define L3_MILLION      ((uint32_t) (1000 * 1000))
#define L3_NS_IN_SEC    ((uint32_t) (1000 * 1000 * 1000))

 // Function prototypes
 void test_logging_perf(const char *logtype, int nMil, const char *filename);

// Convert timespec value to nanoseconds units.
static uint64_t inline
timespec_to_ns(struct timespec *ts)
{
    return ((ts->tv_sec * L3_NS_IN_SEC) + ts->tv_nsec);
}

