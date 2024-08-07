/**
 * source-location-log.cpp
 */
/**
 * log() - Generic logging method to print source-code-location and a msg.
 */
#include <iostream>
#include <source_location>

std::source_location
log(const std::string_view msg,
    const std::source_location loc = std::source_location::current())
{
    std::cout << "\n"
              << loc.file_name()     << ':'
              << loc.line()          << ':'
              << loc.column()        << "::"
              << loc.function_name()
              << ": '" << msg << "'"
              << '\n';

    // Return instance of source_location where this logging happened at.
    std::source_location curr_location = std::source_location::current();

    return curr_location;
}
