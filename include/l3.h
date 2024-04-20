/**
 * *****************************************************************************
 * \file l3.h
 * \author Greg Law
 * \brief L3: Lightweight Logging Library
 * \version 0.1
 * \date 2023-12-24
 *
 * \copyright Copyright (c) 2023
 * *****************************************************************************
 */
#pragma once

#include <stdint.h>

#ifdef L3_LOC_ENABLED
#include "loc.h"
#endif  // L3_LOC_ENABLED

/**
 * Macro to identify unused function argument.
 */
#define L3_ARG_UNUSED (0)

/**
 * Size of circular ring-buffer to track log entries.
 */
#define L3_MAX_SLOTS (16384)

/**
 * Error codes returned by API / interfaces.
 */
/**
 * When l3_init() fails to establish dladdr(), errno is set to this value,
 * with non-zero $rc returned from l3_init().
 */
#define L3_DLADDR_NOT_FOUND_ERRNO   1234

/**
 * \brief Initialise the logging library.
 *
 * \param path [opt] A filename to back the map where the log will be stored.
 *
 * \return 0 on success, or -1 on failure with \c errno set to something appropriate.
 */
#ifdef __cplusplus
extern "C" {
#endif

int l3_init(const char *path);

#ifdef __cplusplus
}
#endif

/**
 * \brief Caller-macro to invoke L3 simple logging.
 */
#ifdef L3_LOC_ENABLED
#define l3_log_simple(msg, arg1, arg2)                          \
        l3__log_simple(__LOC__, (msg), (arg1), (arg2))
#else   // L3_LOC_ENABLED
#define l3_log_simple(msg, arg1, arg2)                          \
        l3__log_simple(L3_ARG_UNUSED, (msg), (arg1), (arg2))
#endif  //

/**
 * \brief Log a literal string and two arguments.
 *
 * \param msg  A literal string to log. (Note: Do -NOT- pass regular char*
 *             pointers that aren't string literal, this won't work!)
 * \param arg1 A first argument to log.
 * \param arg2 A second argument to log.
 *
 * These will be logged to the file given by the \c path argument to
 * l3_init(). To see the log output, run the l3_dump.py utility passing in
 * the names of the log file and the executable.
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef L3_LOC_ENABLED
void l3__log_simple(const loc_t loc, const char *msg,
                    const uint64_t arg1, const uint64_t arg2);
#else
void l3__log_simple(const uint32_t loc, const char *msg,
                    const uint64_t arg1, const uint64_t arg2);
#endif  // L3_LOC_ENABLED

#ifdef __cplusplus
}
#endif

/**
 * \brief Caller-macro to invoke L3 Fast logging.
 */
#if __APPLE__

#define l3_log_fast(msg, arg1, arg2) l3_log_simple(msg, arg1, arg2)

#else   // __APPLE__

#ifdef L3_LOC_ENABLED

#define l3_log_fast(msg, arg1, arg2) l3__log_fast(__LOC__, (msg), (arg1), (arg2))
#else   // L3_LOC_ENABLED
#define l3_log_fast(msg, arg1, arg2) l3__log_fast(L3_ARG_UNUSED, (msg), (arg1), (arg2))
#endif  // L3_LOC_ENABLED

#endif  // __APPLE__

/**
 * \brief Log a message in as fast a way as possible.
 *
 * Like l3_log_simple() but does not take arguments. Only available on x86-64.
 */
#ifdef __cplusplus
extern "C"
#endif

#ifdef L3_LOC_ENABLED
void l3__log_fast(const loc_t loc, const char *msg,
                  const uint64_t arg1, const uint64_t arg2);
#else
void l3__log_fast(const uint32_t loc, const char *msg,
                  const uint64_t arg1, const uint64_t arg2);
#endif  // L3_LOC_ENABLED
