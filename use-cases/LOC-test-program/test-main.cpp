/**
 * *****************************************************************************
 * \file test-main.cpp
 * \author Aditya P. Gurajajada
 * \brief L3_LOC integration unit-test program.
 * \version 0.1
 * \date 2024-04-14
 *
 * All this program does is to assert that the expected typeid() for __LOC__
 * matches the expected type-ID for defined base typedef. This 'test' program
 * exists mainly to debug-through some build issues encountered while
 * developing the integration of the machinery for LOC-ELF encoding with L3's
 * logging APIs.
 *
 * \copyright Copyright (c) 2024
 * *****************************************************************************
 */
#include <iostream>
#include <assert.h>

#include "l3.h"

using namespace std;

int
main(const int argc, const char *argv[])
{
    assert(typeid(__LOC__) == typeid(intptr_t));
    cout << "typeid(__LOC__) == typeid(intptr_t) succeeded.\n";

    loc_t loc_id;

    assert(typeid(__LOC__) != typeid(loc_id));
    cout << "typeid(__LOC__) != typeid(loc_id) succeeded.\n";

    assert(typeid(__LOC__) != typeid(int32_t));
    cout << "typeid(__LOC__) != typeid(int32_t) succeeded.\n";

    assert(sizeof(intptr_t) == sizeof(uint64_t));
}
