/*
 * C++ 20 has intrinsic support for this file/line# combo through the
 * source_location{} object.
 *
 * This sample program is copied from:
 * https://en.cppreference.com/w/cpp/utility/source_location
 */
#include <iostream>
#include <string_view>
#include <source_location>

#include "source-location.h"

/**
 * func() - Some templatized function to invoke generic log()'ging method.
 */
template<typename T>
void func(T x)
{
    std::source_location callee = LOG(x);

    pr_source_location(callee, " [Callee template<T> func()]");
}

int main(const int argc, char * argv[])
{
    std::cout << __func__ << "(): Size of std::source_location: "
              << sizeof(std::source_location) << " bytes"
              << std::endl;

    std::source_location callee = LOG("Hello world!");

    std::cout << callee.file_name()     << ':'
              << callee.line()          << ':'
              << callee.column()        << "::"
              << callee.function_name()
              << " [Callee main()]"
              << '\n';

    func("Hello C++20!");

    minion();
}
