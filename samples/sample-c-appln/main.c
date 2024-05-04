#include <stdio.h>
#include <stdlib.h> // For exit(), EXIT_FAILURE etc.
#include <errno.h>  // For errno

#include "functions.h"

int main(int argc, char ** argv) {
  printf("\n%s:%d::%s() Starting ...\n", __FILE__, __LINE__, __func__);

  call_function();

  printf("\n%s:%d::%s() Completed.\n", __FILE__, __LINE__, __func__);
  return EXIT_SUCCESS;
}
