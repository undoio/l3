/**
 * source-location-utils.cpp
 */
#include <iostream>
#include <source_location>

/**
 * pr_source_location() - Print the contents of a source-code-location object.
 */
void
pr_source_location(const std::source_location loc,
                   const std::string msg = "")
{
    std::cout << loc.file_name()     << ':'
              << loc.line()          << ':'
              << loc.column()        << "::"
              << loc.function_name()
              << ": '" << msg << "'"
              << '\n';
}
