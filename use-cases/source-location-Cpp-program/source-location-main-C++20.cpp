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

std::source_location
log(const std::string_view message,
    const std::source_location location = std::source_location::current())
{
    std::clog << "\n" << location.file_name() << ':'
              << location.line() << ':'
              << location.column() << "::"
              << location.function_name()
              << " [" << sizeof(std::source_location) << "]:"
              << " [sizeof(file_name)="
              << sizeof(location.file_name()) << "]: "
              << "'" << message << "'"
              << '\n';

    std::source_location curr_location = std::source_location::current();

    return curr_location;

}

template<typename T>
void func(T x)
{
    std::source_location callee = log(x);
    std::clog << callee.file_name() << ':'
              << callee.line() << ':'
              << callee.column() << "::"
              << callee.function_name()
              << " [Callee]"
              << '\n';
}

int main(const int argc, char * argv[])
{
    std::cout << __func__ << "(): Size of std::source_location: "
              << sizeof(std::source_location) << " bytes"
              << std::endl;

    std::source_location callee = log("Hello world!");

    std::clog << callee.file_name() << ':'
              << callee.line() << ':'
              << callee.column() << "::"
              << callee.function_name()
              << " [Callee]"
              << '\n';

    func("Hello C++20!");
}
