/** source-location-minion.cpp
 */

#include "source-location.h"

void
minion(void)
{
    std::source_location callee = LOG("Hello from minion");
}
