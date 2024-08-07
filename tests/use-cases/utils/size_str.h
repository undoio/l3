/**
 * *****************************************************************************
 * utils/size_str.h
 *
 * Format a size value with unit-specifiers, in an output buffer.
 *
 * Extracted as standalone utility sources from original implementation done by
 * the author in the SplinterDB project.
 * Ref: https://github.com/vmware/splinterdb
 *
 * Author: Aditya P. Gurajada
 * Copyright (c) 2024
 * *****************************************************************************
 */
#pragma once

// Data unit constants
#define SZ_KiB (1024UL)
#define SZ_MiB (SZ_KiB * 1024)
#define SZ_GiB (SZ_MiB * 1024)
#define SZ_TiB (SZ_GiB * 1024)

// Convert 'x' in unit-specifiers to bytes
#define SZ_KiB_TO_B(x) ((x) * SZ_KiB)
#define SZ_MiB_TO_B(x) ((x) * SZ_MiB)
#define SZ_GiB_TO_B(x) ((x) * SZ_GiB)
#define SZ_TiB_TO_B(x) ((x) * SZ_TiB)

// Convert 'x' in bytes to 'int'-value with unit-specifiers
#define SZ_B_TO_KiB(x) ((x) / SZ_KiB)
#define SZ_B_TO_MiB(x) ((x) / SZ_MiB)
#define SZ_B_TO_GiB(x) ((x) / SZ_GiB)
#define SZ_B_TO_TiB(x) ((x) / SZ_TiB)

// For x bytes, returns as int the fractional portion modulo a unit-specifier
#define SZ_B_TO_KiB_FRACT(x) ((100 * ((x) % SZ_KiB)) / SZ_KiB)
#define SZ_B_TO_MiB_FRACT(x) ((100 * ((x) % SZ_MiB)) / SZ_MiB)
#define SZ_B_TO_GiB_FRACT(x) ((100 * ((x) % SZ_GiB)) / SZ_GiB)
#define SZ_B_TO_TiB_FRACT(x) ((100 * ((x) % SZ_TiB)) / SZ_TiB)

// Value unit constants
#define VAL_OneK        (1000UL)  // Use 'OneK' for convenience; it's actually 1000
#define VAL_Million     (1000 * VAL_OneK)
#define VAL_Billion     (1000 * VAL_Million)
#define VAL_Trillion    (1000 * VAL_Billion)

// Convert 'x' in unit-specifiers to value as a number, N
#define VAL_K_TO_N(x)           ((x) * VAL_OneK)
#define VAL_Million_TO_N(x)     ((x) * VAL_Million)
#define VAL_Billion_TO_N(x)     ((x) * VAL_Billion)
#define VAL_Trillion_TO_N(x)    ((x) * VAL_Trillion)

// Convert 'x' as a number-value to 'int'-value with unit-specifiers
#define VAL_N_TO_K(x)           ((x) / VAL_OneK)
#define VAL_N_TO_Million(x)     ((x) / VAL_Million)
#define VAL_N_TO_Billion(x)     ((x) / VAL_Billion)
#define VAL_N_TO_Trillion(x)    ((x) / VAL_Trillion)

// For x as a number-value, returns as int the fractional portion modulo
// a unit-specifier
#define VAL_N_TO_K_FRACT(x)         ((100 * ((x) % VAL_OneK))     / VAL_OneK)
#define VAL_N_TO_Million_FRACT(x)   ((100 * ((x) % VAL_Million))  / VAL_Million)
#define VAL_N_TO_Billion_FRACT(x)   ((100 * ((x) % VAL_Billion))  / VAL_Billion)
#define VAL_N_TO_Trillion_FRACT(x)  ((100 * ((x) % VAL_Trillion)) / VAL_Trillion)

// ----------------------------------------------------------------------
// Format a size value with unit-specifiers, in an output buffer.
// ----------------------------------------------------------------------

char *
size_to_str(char *outbuf, size_t outbuflen, size_t size);

char *
size_to_fmtstr(char *outbuf, size_t outbuflen, const char *fmtstr, size_t size);

// Length of output buffer to snprintf()-into size as string w/ unit specifier
#define SIZE_TO_STR_LEN 25

/*
 * ----------------------------------------------------------------------
 * Convenience caller macros to convert 'sz' bytes to return a string,
 * formatting the input size as human-readable value with unit-specifiers.
 * ----------------------------------------------------------------------
 */
// char *size_str(size_t sz)
#define size_str(sz)                                                           \
   (({                                                                         \
       struct {                                                                \
          char buffer[SIZE_TO_STR_LEN];                                        \
       } onstack_chartmp;                                                      \
       size_to_str(                                                            \
          onstack_chartmp.buffer, sizeof(onstack_chartmp.buffer), sz);         \
       onstack_chartmp;                                                        \
    }).buffer)

// char *size_fmtstr(const char *fmtstr, size_t sz)
#define size_fmtstr(fmtstr, sz)                                                \
   (({                                                                         \
       struct {                                                                \
          char buffer[SIZE_TO_STR_LEN];                                        \
       } onstack_chartmp;                                                      \
       size_to_fmtstr(                                                         \
          onstack_chartmp.buffer, sizeof(onstack_chartmp.buffer), fmtstr, sz); \
       onstack_chartmp;                                                        \
    }).buffer)


// ----------------------------------------------------------------------
// Format a number-value with unit-specifiers, in an output buffer.
// ----------------------------------------------------------------------

char *
value_to_str(char *outbuf, size_t outbuflen, size_t size);

char *
value_to_fmtstr(char *outbuf, size_t outbuflen, const char *fmtstr, size_t size);


/*
 * ----------------------------------------------------------------------
 * Convenience caller macros to convert 'val' number-value to return a string,
 * formatting the input size as human-readable value with unit-specifiers.
 * ----------------------------------------------------------------------
 */
// char *value_str(size_t val)
#define value_str(val)                                                          \
   (({                                                                          \
       struct {                                                                 \
          char buffer[SIZE_TO_STR_LEN];                                         \
       } onstack_chartmp;                                                       \
       value_to_str(                                                            \
          onstack_chartmp.buffer, sizeof(onstack_chartmp.buffer), val);         \
       onstack_chartmp;                                                         \
    }).buffer)

// char *value_fmtstr(const char *fmtstr, size_t val)
#define value_fmtstr(fmtstr, val)                                               \
   (({                                                                          \
       struct {                                                                 \
          char buffer[SIZE_TO_STR_LEN];                                         \
       } onstack_chartmp;                                                       \
       value_to_fmtstr(                                                         \
          onstack_chartmp.buffer, sizeof(onstack_chartmp.buffer), fmtstr, val); \
       onstack_chartmp;                                                         \
    }).buffer)
