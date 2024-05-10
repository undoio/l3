/**
 * *****************************************************************************
 * unit/size_str-test.c
 *
 * Simple unit-test to exercise the size-formatting interfaces in size_str.c
 *
 * These interfaces have already been unit-tested in the upstream repo
 * sources. All we do here is to exercise the conversion APIs with a set of
 * unit-test cases, expanded to also cover the value_to_str() and value_fmtstr()
 * interfaces, just to make sure that things run, and don't fall-over.
 *
 * Author: Aditya P. Gurajada
 * Copyright (c) 2024
 * *****************************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "size_str.h"

#define ARRAY_SIZE(arr) (int) (sizeof(arr) / sizeof(*arr))

_Bool test_streqn(const char *str, const char *expstr);

int
main(const int argc, const char **argv)
{
   typedef struct size_to_expstr {
      size_t size;
      char  *expstr_b;  // size treated as bytes converted to string.
      char  *expstr_v;  // size treated as value converted to string.
   } size_to_expstr;

   // Exercise the string formatter with typical data values.
   size_to_expstr size_to_str_cases[] =
   {
        // size                                                     bytes-as-str            value-as-str
          { 129                                                 , "129 bytes"           , ""                }   //  0

        , { SZ_KiB                                              , "1 KiB"               , "~1.02 K"         }   //  1
        , { VAL_OneK                                            , "1000 bytes"          , "1 K"             }   //  2
        , { SZ_KiB + 128                                        , "~1.12 KiB"           , "~1.15 K"         }   //  3
        , { SZ_KiB + ((25 * SZ_KiB) / 100)                      , "~1.25 KiB"           , "~1.28 K"         }   //  4
        , { ( 2 * SZ_KiB )                                      , "2 KiB"               , "~2.04 K"         }   //  5
        , { ( 2 * VAL_OneK )                                    , "~1.95 KiB"           , "2 K"             }   //  6

        , { VAL_Million                                         , "~976.56 KiB"         , "1 Million"       }   //  7
        , { SZ_MiB                                              , "1 MiB"               , "~1.04 Million"   }   //  8
        , { SZ_MiB + 128                                        , "~1.00 MiB"           , "~1.04 Million"   }   //  9
        , { SZ_MiB + ((5 * SZ_MiB) / 10)                        , "~1.50 MiB"           , "~1.57 Million"   }   // 10
        , { ( VAL_Million + VAL_OneK )                          , "~977.53 KiB"         , "~1.00 Million"   }   // 11

        , { ( VAL_Billion - VAL_Million - ( 2 * VAL_OneK ) )    , "~952.71 MiB"         , "~998.99 Million" }   // 12
        , { VAL_Billion                                         , "~953.67 MiB"         , "1 Billion"       }   // 13
        , { ( VAL_Billion + VAL_Million + ( 3 * VAL_OneK ) )    , "~954.63 MiB"         , "~1.00 Billion"   }   // 14
        , { SZ_GiB                                              , "1 GiB"               , "~1.07 Billion"   }   // 15
        , { SZ_GiB + 128                                        , "~1.00 GiB"           , "~1.07 Billion"   }   // 16
        , { SZ_GiB + ((75 * SZ_GiB) / 100)                      , "~1.75 GiB"           , "~1.87 Billion"   }   // 17
        , { (3 * SZ_GiB) + ((5 * SZ_GiB) / 10)                  , "~3.50 GiB"           , "~3.75 Billion"   }   // 18

        , { VAL_Trillion                                        , "~931.32 GiB"         , "1 Trillion"      }   // 19
        , { SZ_TiB                                              , "1 TiB"               , "~1.09 Trillion"  }   // 20
        , { SZ_TiB + 128                                        , "~1.00 TiB"           , "~1.09 Trillion"  }   // 21
        , { (2 * SZ_TiB) + ((25 * SZ_TiB) / 100)                , "~2.25 TiB"           , "~2.47 Trillion"  }   // 22

        // Specific data-values that tripped bugs in formatting output string
        , { 2222981120                                          , "~2.07 GiB"           , "~2.22 Billion"   }   // 23
        , { SZ_KiB + 28                                         , "~1.02 KiB"           , "~1.05 K"         }   // 24
        , { SZ_MiB + (98 * SZ_KiB)                              , "~1.09 MiB"           , "~1.14 Million"   }   // 25
        , { SZ_GiB + (555 * SZ_MiB)                             , "~1.54 GiB"           , "~1.65 Billion"   }   // 26
   };

   char size_str[SIZE_TO_STR_LEN];
   char *outstr = NULL;
   const char *expstr_b;
   const char *expstr_v;

   for (int ictr = 0; ictr < ARRAY_SIZE(size_to_str_cases); ictr++) {
       size_t value = size_to_str_cases[ictr].size;
       printf(" [%2d] Size  = %lu (%s)"
              " "
              "  Value = %lu (%s)\n",
              ictr,
              value, size_str(value),
              value, value_fmtstr("%s", value));

        expstr_b = size_to_str_cases[ictr].expstr_b;
        expstr_v = size_to_str_cases[ictr].expstr_v;

        outstr = size_to_str(size_str, sizeof(size_str), value);
        assert(test_streqn(outstr, expstr_b));

        outstr = value_to_str(size_str, sizeof(size_str), value);
        assert(test_streqn(outstr, expstr_v));
   }
}

// Simple string comparison method. Print diagnostics upon failure.
_Bool
test_streqn(const char *str, const char *expstr)
{
    _Bool rv = true;
    size_t str_len = strlen(str);
    if (str_len != strlen(expstr)) {
        rv = false;
    } else if (strncmp(str, expstr, str_len) != 0) {
        rv = false;
    }
    if (!rv) {
        printf("String '%s' does not match expected string '%s'\n",
               str, expstr);

    }
    return rv;
}
