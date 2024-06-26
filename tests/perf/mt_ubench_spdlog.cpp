#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <stdlib.h>

#include <pthread.h>

std::shared_ptr<spdlog::logger> logger;

pthread_barrier_t barrier;


int main(int argc, char** argv) {
    int nthreads = argc == 1 ? 10 : atoi(argv[1]);
    pthread_barrier_init(&barrier, NULL, nthreads+1);
    pthread_t threads[nthreads];
    logger = spdlog::basic_logger_mt("file_logger", "/tmp/logger.txt", true);
    logger->enable_backtrace(200);
    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, [](void*) -> void* {
            int tid = syscall(SYS_gettid);
            pthread_barrier_wait(&barrier);
            for (int j = 0; j < 1000000; j++) {
                logger->info("{}: Hello, World! tid: {}", j, tid);
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
