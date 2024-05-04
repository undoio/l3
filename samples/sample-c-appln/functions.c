#include <stdio.h>
#include "l3.h"

void call_function() {
  printf("\n%s:%d::%s() Hello World!\n", __FILE__, __LINE__, __func__);

#ifdef L3_LOC_ENABLED
  loc_t loc = __LOC__;
  printf("\nLOC-macros: __LOC__ = %d, LOC_FILE = '%s', LOC_LINE = %d\n",
         loc, LOC_FILE(loc), LOC_LINE(loc));
#endif  // L3_LOC_ENABLED

  l3_log_simple("Hello World with L3-logging, addr=%p, size=%d bytes",
                0xdeadbeef, 42);

  l3_log_fast("Fast L3-logging, with assembly support, bp=%p, refcount=%d",
                0xfadedeaf, 2);
}
