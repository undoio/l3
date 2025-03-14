#include <stdio.h>
#include "l3.h"

void call_function() {
  printf("\n%s:%d::%s() Hello World!\n", __FILE__, __LINE__, __func__);
  l3_log("Hello World with L3-logging!");
  l3_log("Format test: addr=%p, size=%d bytes.", 0xdeadbeef, 42);
  l3_log("Long test: %lu %lu %lu", 0xeeeeeeeeeeeeeeee, 0xdddddddddddddddd, 0x1111111111111111);
  l3_log("Nine args: %d %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8, 9);
  l3_log("Sample is now over%c%c%c", '!', '!', '?');

#ifdef L3_LOC_ENABLED
  loc_t loc = __LOC__;
  printf("\nLOC-macros: __LOC__ = %d, LOC_FILE = '%s', LOC_LINE = %d\n",
         loc, LOC_FILE(loc), LOC_LINE(loc));
#endif  // L3_LOC_ENABLED

}
