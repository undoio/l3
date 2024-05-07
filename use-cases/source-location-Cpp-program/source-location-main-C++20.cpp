/*
 * C++ 20 has intrinsic support for this file/line# combo through the
 * source_location{} object.
 *
 * This sample program is copied from:
 * https://en.cppreference.com/w/cpp/utility/source_location
 */
#include <iostream>
#include <source_location>
#include <string_view>

void log(const std::string_view message,
         const std::source_location location =
               std::source_location::current())
{
    std::clog << "file: "
              << location.file_name() << '('
              << location.line() << ':'
              << location.column() << ") "
              << location.function_name() << ": "
              << message << '\n';
}

template<typename T>
void func(T x)
{
    log(x);
}

int main(int, char*[])
{
    log("Hello world!");
    func("Hello C++20!");
}
