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

    std::clog << curr_location.file_name() << ':'
              << curr_location.line() << ':'
              << curr_location.column() << "::"
              << curr_location.function_name()
              // << " [" << sizeof(std::source_location) << "]: "
              // << " [" << sizeof(curr_location.file_name()) << "]: "
              // << message
              << " [Callee]"
              << '\n';

}

template<typename T>
void func(T x)
{
    log(x);
}

int main(const int argc, char * argv[])
{
    std::cout << __func__ << "(): Size of std::source_location: "
              << sizeof(std::source_location) << " bytes"
              << std::endl;

    log("Hello world!");
    func("Hello C++20!");
}
