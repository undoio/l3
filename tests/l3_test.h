/**
 * *****************************************************************************
 * \file l3_test.h
 * \author Greg Law, Aditya P. Gurajada
 * \brief L3: Lightweight Logging Library
 * \version 0.1
 * \date 2024-08-08
 *
 * \copyright Copyright (c) 2024
 * *****************************************************************************
 */

#pragma once

#include <stdio.h>

#include "l3.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Different logging interfaces, mainly intended for use by micro-benchmarking
 * utility programs.
 */
typedef enum {
      L3_LOG_UNDEF      = 0
    , L3_LOG_MMAP
    , L3_LOG_FPRINTF
    , L3_LOG_WRITE
    , L3_LOG_WRITE_MSG
    , L3_LOGTYPE_MAX
    , L3_LOG_DEFAULT    = L3_LOG_MMAP
} l3_test_log_t;

int l3_test_log_init(const l3_test_log_t logtype, const char *path);

int l3_test_log_deinit(const l3_test_log_t logtype);
const char *l3_test_logtype_name(l3_test_log_t logtype);

#if defined(DEBUG)
    #if defined(L3_LOGT_FPRINTF)

        #  undef l3_log
        #  define l3_log(msg, arg1, arg2) l3_log_fprintf((msg), arg1, arg2)

    #elif defined(L3_LOGT_WRITE)

        #  undef l3_log
        #  define l3_log(msg, arg1, arg2) l3_log_write((msg), arg1, arg2)

    #endif

  #else   // (DEBUG)

    #if defined(L3_LOGT_FPRINTF)

        #  undef l3_log
        #  define l3_log(msg, arg1, arg2) l3_log_fprintf((msg), arg1, arg2)

    #elif defined(L3_LOGT_WRITE)

        #  undef l3_log
        #  define l3_log(msg, arg1, arg2) l3_log_write((msg), arg1, arg2)
    #endif

#endif

void l3_log_write(const char *msgfmt, const uint64_t arg1, const uint64_t arg2);
void l3_log_fprintf(const char *msg, const uint64_t arg1, const uint64_t arg2);


#ifdef __cplusplus
}
#endif

