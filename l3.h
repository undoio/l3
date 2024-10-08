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

// Define version of static assertion checking that works for both gcc and g++
#ifdef __cplusplus
#define L3_STATIC_ASSERT    static_assert
#else
#define L3_STATIC_ASSERT    _Static_assert
#endif

#define L3_ARRAY_LEN(arr)   (sizeof(arr) / sizeof(*arr))

#define L3_MIN(a, b)        ((a) > (b) ? (b) : (a))


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

/**
 * \brief Deinitialise the L3 logging library.
 *
 * \return 0 on success, or -1 on failure with \c errno set to something appropriate.
 */
int l3_deinit(void);

#ifdef __cplusplus
}
#endif

/**
 * \brief Caller-macro to invoke L3 logging.
 *
 * Macro to synthesize LOC-encoding value, if needed, under conditional
 * compilation flags. DEBUG-version of macros are provided to cross-check
 * that the supplied format string in `msg` are consistent with the
 * arguments' type.
 */
#if defined(DEBUG)

  #ifdef L3_LOC_ENABLED

    #define l3_log(msg, arg1, arg2)                                     \
            if (1) {                                                    \
                l3_log_fn((msg),                                      \
                            (uint64_t) (arg1), (uint64_t) (arg2),       \
                            __LOC__);                                   \
            } else if (0) {                                             \
                printf((msg), (arg1), (arg2));                          \
            } else

   #else   // L3_LOC_ENABLED

    #  define l3_log(msg, arg1, arg2)                                   \
            if (1) {                                                    \
                l3_log_fn((msg),                                      \
                            (uint64_t) (arg1), (uint64_t) (arg2),       \
                            L3_ARG_UNUSED);                             \
            } else if (0) {                                             \
                printf((msg), (arg1), (arg2));                          \
            } else

  #endif  // L3_LOC_ENABLED

#else   // defined(DEBUG)

  #ifdef L3_LOC_ENABLED

    #define l3_log(msg, arg1, arg2)                                     \
            l3_log_fn((msg),                                          \
                        (uint64_t) (arg1), (uint64_t) (arg2),           \
                        __LOC__)

    #else
    #define l3_log(msg, arg1, arg2)                                     \
            l3_log_fn((msg),                                          \
                        (uint64_t) (arg1), (uint64_t) (arg2),           \
                        L3_ARG_UNUSED)

  #endif  // L3_LOC_ENABLED

#endif  // defined(DEBUG)

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
void l3_log_fn(const char *msg, const uint64_t arg1, const uint64_t arg2,
                 const loc_t loc);
#else
void l3_log_fn(const char *msg, const uint64_t arg1, const uint64_t arg2,
                 const uint32_t loc);
#endif  // L3_LOC_ENABLED

#ifdef __cplusplus
}
#endif
