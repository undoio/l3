/** source-location-minion.cpp
 */
#include <string>
#include "source-location.h"

void
minion(void)
{
    std::source_location callee = LOG("Hello from minion");
    pr_source_location(callee, " [Callee minion()]");
}
