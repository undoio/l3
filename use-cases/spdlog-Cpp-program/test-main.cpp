/**
 * *****************************************************************************
 * spdlog-Cpp-program/test-main.cpp: spdlog example program.
 *
 * Developed based on docs / examples from spdlog repo
 * References:
 *  [1] https://github.com/gabime/spdlog
 *  [2] https://github.com/gabime/spdlog/blob/v1.x/example/example.cpp
 *
 * ---- Build Instructions: ----
 *
 * $ sudo apt-get install -y libfmt-dev/jammy   # Pre-requisite 'fmt' library
 * $ sudo apt-get install -y libspdlog-dev
 *
 * $ cd ~/Projects/
 * $ git clone https://github.com/gabime/spdlog.git
 * $ cd spdlog && mkdir build && cd build
 * $ cmake .. && make -j
 *
 * Now build this test program using the library built above.
 * $ g++ -I /usr/include/spdlog -o spdlog-Cpp-program test-main.cpp -L ~/Projects/spdlog/build -l spdlog -l fmt
 * *****************************************************************************
 */
#include <iostream>
#include "spdlog/spdlog.h"

#include "spdlog/sinks/basic_file_sink.h"
using namespace std;

void
basic_example(void)
{
    // Create basic file logger (not rotated).
    string logfile = "/tmp/basic-log.txt";
    auto my_logger = spdlog::basic_logger_mt("file_logger", logfile, true);
    my_logger->info("Welcome to spdlog!");
    my_logger->critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    cout << "spdlog messages can be found in '" << logfile << "'\n";
}

/**
 * Debug messages can be stored in a ring buffer instead of being logged
 * immediately. This is useful to display debug logs only when needed
 * (e.g. when an error happens).
 * When needed, call dump_backtrace() to dump them to your log.
 */
void
backtrace_example(void)
{
    spdlog::enable_backtrace(32); // Store the latest 32 messages in a buffer.
    // or my_logger->enable_backtrace(32)..
    for(int i = 0; i < 100; i++)
    {
        spdlog::debug("Backtrace message {}", i); // not logged yet..
    }
    // e.g. if some error happened:
    spdlog::dump_backtrace(); // log them now! show the last 32 messages
    // or my_logger->dump_backtrace(32)..
}

int
main()
{
    cout << "Hello world\n";
    spdlog::info("Welcome to spdlog!");
    spdlog::error("Some error message with arg: {}", 1);
    spdlog::warn("Easy padding in numbers like {:08d}", 12);
    spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    spdlog::info("Support for floats {:03.2f}", 1.23456);
    spdlog::info("Positional args are {1} {0}..", "too", "supported");
    spdlog::info("{:<30}", "left aligned");

    backtrace_example();

    basic_example();
}
