/*
 * C++ 20 has intrinsic support for this file/line# combo through the
 * source_location{} object.
 */
#pragma once

#include <string_view>
#include <source_location>

// Caller-macro to synthesize current source_location as 'location' arg.
#define LOG(msg)    log((msg), std::source_location::current())

// Function protoypes

std::source_location
log(const std::string_view message,
    const std::source_location location);

void minion(void);
