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
 * A message occupies 1 slot, and 1 more for each 3 arguments.
 */
#define L3_MAX_SLOTS (32768)

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

#define L3_PREFIX_CAST(x) ((uint64_t)(x))

#define L3_MACRO_NARGS_IMPL(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,N,...) N
#define L3_MACRO_NARGS(...) L3_MACRO_NARGS_IMPL(_, ##__VA_ARGS__, 9,8,7,6,5,4,3,2,1,0) // GCC/Clang extension
// Deeply nested macro expansion such that it appears recursive
#define L3_MACRO_EXPAND(...)  L3_MACRO_EXPAND2(L3_MACRO_EXPAND2(L3_MACRO_EXPAND2(L3_MACRO_EXPAND2(__VA_ARGS__))))
#define L3_MACRO_EXPAND2(...) L3_MACRO_EXPAND1(L3_MACRO_EXPAND1(L3_MACRO_EXPAND1(L3_MACRO_EXPAND1(__VA_ARGS__))))
#define L3_MACRO_EXPAND1(...) __VA_ARGS__
#define L3_MACRO_PARENS ()
#define L3_MACRO_FOR_EACH(macro, ...) __VA_OPT__(L3_MACRO_EXPAND(L3_MACRO_FOR_EACH_LOOP(macro, __VA_ARGS__)))
#define L3_MACRO_FOR_EACH_LOOP(macro, a1, ...) macro(a1) __VA_OPT__(, L3_MACRO_FOR_EACH_LOOP2 L3_MACRO_PARENS (macro, __VA_ARGS__))
#define L3_MACRO_FOR_EACH_LOOP2() L3_MACRO_FOR_EACH_LOOP
#define L3_PREFIX_MULTI(...) L3_MACRO_FOR_EACH(L3_PREFIX_CAST, __VA_ARGS__)

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
                l3_log_fn((msg), __LOC__, L3_MACRO_NARGS(__VA_ARGS__) __VA_OPT__(,) L3_PREFIX_MULTI(__VA_ARGS__));\
            } else if (0) {                                             \
                printf((msg), L3_PREFIX_MULTI(__VA_ARGS__));                          \
            } else

   #else   // L3_LOC_ENABLED

    #  define l3_log(msg, arg1, arg2)                                   \
            if (1) {                                                    \
                l3_log_fn((msg), L3_ARG_UNUSED, L3_MACRO_NARGS(__VA_ARGS__) __VA_OPT__(,) L3_PREFIX_MULTI(__VA_ARGS__));\
            } else if (0) {                                             \
                printf((msg), L3_PREFIX_MULTI(__VA_ARGS__));                          \
            } else

  #endif  // L3_LOC_ENABLED

#else   // defined(DEBUG)

  #ifdef L3_LOC_ENABLED

    #define l3_log(msg, arg1, arg2)                                     \
            l3_log_fn((msg), __LOC__, L3_MACRO_NARGS(__VA_ARGS__) __VA_OPT__(,) L3_PREFIX_MULTI(__VA_ARGS__))

    #else
    #define l3_log(msg, ...) \
            l3_log_fn((msg), L3_ARG_UNUSED, L3_MACRO_NARGS(__VA_ARGS__) __VA_OPT__(,) L3_PREFIX_MULTI(__VA_ARGS__))

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
void l3_log_fn(const char *msg, loc_t loc, int nargs, ...);
#else
void l3_log_fn(const char *msg, uint32_t loc, int nargs, ...);
#endif  // L3_LOC_ENABLED

#ifdef __cplusplus
}
#endif
