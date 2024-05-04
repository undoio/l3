#include <stdio.h>

void call_function() {
  printf("\n%s:%d::%s() Hello World!\n", __FILE__, __LINE__, __func__);
}
