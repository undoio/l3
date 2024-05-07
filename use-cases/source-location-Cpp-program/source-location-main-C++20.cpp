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

/**
 * log() - Generic logging method to print source-code-location and a msg.
 */
std::source_location
log(const std::string_view message,
    const std::source_location location = std::source_location::current())
{
    std::cout << "\n" << location.file_name() << ':'
              << location.line() << ':'
              << location.column() << "::"
              << location.function_name()
              << " [" << sizeof(std::source_location) << "]:"
              << " [sizeof(file_name)="
              << sizeof(location.file_name()) << "]: "
              << "'" << message << "'"
              << '\n';

    // Return instance of source_location where this logging happened at.
    std::source_location curr_location = std::source_location::current();

    return curr_location;

}

/**
 * pr_source_location() - Print the contents of a source-code-location object.
 */
void
pr_source_location(const std::source_location loc,
                   const std::string msg = "")
{
    std::cout << loc.file_name() << ':'
              << loc.line() << ':'
              << loc.column() << "::"
              << loc.function_name()
              << msg
              << '\n';
}

/**
 * func() - Some templatized function to invoke generic log()'ging method.
 */
template<typename T>
void func(T x)
{
    std::source_location callee = log(x);

    pr_source_location(callee, " [Callee]");
}

int main(const int argc, char * argv[])
{
    std::cout << __func__ << "(): Size of std::source_location: "
              << sizeof(std::source_location) << " bytes"
              << std::endl;

    std::source_location callee = log("Hello world!");

    std::cout << callee.file_name() << ':'
              << callee.line() << ':'
              << callee.column() << "::"
              << callee.function_name()
              << " [Callee]"
              << '\n';

    func("Hello C++20!");
}
