/** source-location-minion.cpp
 */
#include <string>
#include "source-location.h"

/**
 * minion() : Shows an example usage of LOG() interface where we can silently
 * ignore the returned std::source_location.
 * There is no hard requirement for caller to save that returned handle.
 */
void
minion(void)
{
    // Example: You do -not- need to do this code construction:
    // std::source_location callee = LOG("Hello from minion");
    // pr_source_location(callee, " [Callee: minion()]");

    // Simply invoke LOG() to get this source location's info
    LOG("Hello from minion - skip returned source_location handle.");
}
