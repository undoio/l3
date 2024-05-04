#include <stdio.h>
#include <stdlib.h> // For exit(), EXIT_FAILURE etc.
#include <errno.h>  // For errno

#include "functions.h"
#include "l3.h"

int main(int argc, char ** argv) {
  printf("\n%s:%d::%s() Starting ...\n", __FILE__, __LINE__, __func__);

  // Initialize L3-logging
  const char *logfile = "/tmp/c-sample-test.dat";
  int e = l3_init(logfile);
  if (e) {
      printf("Error initializing L3-logging system: errno=%d\n", errno);
      exit(EXIT_FAILURE);
  }
  printf("\nInitialized L3-logging to mmap()'ed file '%s'\n", logfile);

  call_function();

  printf("\n%s:%d::%s() Completed.\n", __FILE__, __LINE__, __func__);
  return EXIT_SUCCESS;
}
