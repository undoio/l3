/**
 * *****************************************************************************
 * utils/size_str.c
 *
 * Format a size / number-value with unit-specifiers, in an output buffer.
 *
 * - Extracted the implementation of size_to_str() and associated caller-macros,
 *   as standalone utility sources from the original implementation done
 *   by the author in the SplinterDB project.
 *
 * - Expanded interface to also convert number-value(s) to a string with unit-
 *   specifiers.
 *
 * Ref: https://github.com/vmware/splinterdb
 *
 * Author: Aditya P. Gurajada
 * Copyright (c) 2024
 * *****************************************************************************
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "size_str.h"

/*
 * *****************************************************************************
 * Format a size value with unit-specifiers, in an output buffer.
 * Returns 'outbuf', as ptr to size-value snprintf()'ed as a string.
 * *****************************************************************************
 */
char *
size_to_str(char *outbuf, size_t outbuflen, size_t size)
{
   assert(outbuflen >= SIZE_TO_STR_LEN);
   size_t unit_val  = 0;
   size_t frac_val  = 0;
   int is_approx = false;

   char *units = NULL;
   if (size >= SZ_TiB) {
      unit_val  = SZ_B_TO_TiB(size);
      frac_val  = SZ_B_TO_TiB_FRACT(size);
      is_approx = (size > SZ_TiB_TO_B(unit_val));
      units     = "TiB";

   } else if (size >= SZ_GiB) {
      unit_val  = SZ_B_TO_GiB(size);
      frac_val  = SZ_B_TO_GiB_FRACT(size);
      is_approx = (size > SZ_GiB_TO_B(unit_val));
      units     = "GiB";

   } else if (size >= SZ_MiB) {
      unit_val  = SZ_B_TO_MiB(size);
      frac_val  = SZ_B_TO_MiB_FRACT(size);
      is_approx = (size > SZ_MiB_TO_B(unit_val));
      units     = "MiB";

   } else if (size >= SZ_KiB) {
      unit_val  = SZ_B_TO_KiB(size);
      frac_val  = SZ_B_TO_KiB_FRACT(size);
      is_approx = (size > SZ_KiB_TO_B(unit_val));
      units     = "KiB";
   } else {
      unit_val = size;
      units    = "bytes";
   }

   if (frac_val || is_approx) {
      snprintf(outbuf, outbuflen, "~%ld.%02ld %s", unit_val, frac_val, units);
   } else {
      snprintf(outbuf, outbuflen, "%ld %s", unit_val, units);
   }
   return outbuf;
}

/*
 * Sibling of size_to_str(), but uses user-provided print format specifier.
 * 'fmtstr' is expected to have just one '%s', and whatever other text user
 * wishes to print with the output string.
 */
char *
size_to_fmtstr(char *outbuf, size_t outbuflen, const char *fmtstr, size_t size)
{
   snprintf(outbuf, outbuflen, fmtstr, size_str(size));
   return outbuf;
}

/*
 * *****************************************************************************
 * Format a number value with unit-specifiers, in an output buffer.
 * Returns 'outbuf', as ptr to number-value snprintf()'ed as a string.
 * *****************************************************************************
 */
char *
value_to_str(char *outbuf, size_t outbuflen, size_t value)
{
   assert(outbuflen >= SIZE_TO_STR_LEN);
   size_t unit_val  = 0;
   size_t frac_val  = 0;
   int is_approx = false;

   char *units = NULL;
   if (value >= VAL_Trillion) {
      unit_val  = VAL_N_TO_Trillion(value);
      frac_val  = VAL_N_TO_Trillion_FRACT(value);
      is_approx = (value > VAL_Trillion_TO_N(unit_val));
      units     = "Trillion";

   } else if (value >= VAL_Billion) {
      unit_val  = VAL_N_TO_Billion(value);
      frac_val  = VAL_N_TO_Billion_FRACT(value);
      is_approx = (value > VAL_Billion_TO_N(unit_val));
      units     = "Billion";

   } else if (value >= VAL_Million) {
      unit_val  = VAL_N_TO_Million(value);
      frac_val  = VAL_N_TO_Million_FRACT(value);
      is_approx = (value > VAL_Million_TO_N(unit_val));
      units     = "Million";

   } else if (value >= VAL_OneK) {
      unit_val  = VAL_N_TO_K(value);
      frac_val  = VAL_N_TO_K_FRACT(value);
      is_approx = (value > VAL_K_TO_N(unit_val));
      units     = "K";
   } else {
      unit_val = value;
   }

   if (frac_val || is_approx) {
      snprintf(outbuf, outbuflen, "~%ld.%02ld %s", unit_val, frac_val, units);
   } else if (units) {
      snprintf(outbuf, outbuflen, "%ld %s", unit_val, units);
   } else {
       // If value is less than 1K, no need to further output 'value>' in buffer.
       *outbuf = '\0';
   }
   return outbuf;
}

/*
 * Sibling of value_to_str(), but uses user-provided print format specifier.
 * 'fmtstr' is expected to have just one '%s', and whatever other text user
 * wishes to print with the output string.
 */
char *
value_to_fmtstr(char *outbuf, size_t outbuflen, const char *fmtstr, size_t value)
{
   snprintf(outbuf, outbuflen, fmtstr, value_str(value));
   return outbuf;
}
