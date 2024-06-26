#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "l3.h"

int gettid() {
    return syscall(SYS_gettid);
}

pthread_barrier_t barrier;

int main(int argc, char** argv) {
    int nthreads = argc == 1 ? 10 : atoi(argv[1]);
    pthread_t threads[nthreads];
    int e = pthread_barrier_init(&barrier, NULL, nthreads+1);
    assert(!e);
    l3_init("/tmp/l3.log");
    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, [](void*) -> void* {
            int e = pthread_barrier_wait(&barrier);
            assert(e == 0 || e == PTHREAD_BARRIER_SERIAL_THREAD);
            for (int j = 0; j < 1000000; j++) {
                l3_log_fast("Hello, world! %d %d", 0, j);
            }
            return nullptr;
        }, nullptr);
    }
        pthread_barrier_wait(&barrier);

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}
