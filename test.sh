#!/bin/bash
# ##############################################################################
# test.sh - Driver script to build-and-test L3 toolset.
# This script issues the same kinds of commands that are executed in
# .github/workflows/build.yml . So, yes, there is some duplication, but this
# script is provided for developers to test the changes before invoking CI.
# ##############################################################################

Me=$(basename "$0")
set -euo pipefail

# #############################################################################
# Exercise some error-check conditions
# #############################################################################
# Exercise the check in ./l3_dump.py which verifies that the LOC-decoder
# binary exists for LOC-encoding output data. If not, it should fail with an
# error message.
# #############################################################################
function test_l3_dump_py_missing_loc_decoder()
{
    set +e

    local test_prog="./build/release/bin/test-use-cases/single-file-C-program_loc"
    local tmp_prog="${test_prog}.bak"
    mv "${test_prog}" "${tmp_prog}"

    L3_LOC_ENABLED=1 ./l3_dump.py /tmp/l3.c-small-test.dat ${test_prog}

    mv "${tmp_prog}" "${test_prog}"
    set -e
    echo " "
    echo "${Me}: Expected failure in ./l3_dump.py execution. Continuiing ..."
    echo " "
}

# Currently, this is a simplistic driver, just executing basic `make` commands
# to ensure that all code / tools build correctly in release mode.

echo " "
echo "${Me}: Run build-and-test for core L3 package and tests"
echo " "
set -x
make clean && CC=gcc LD=g++ make all-c-tests
make run-c-tests

make clean-l3 && CC=g++ CXX=g++ LD=g++ make all-cpp-tests
make run-cpp-tests

make clean-l3 && CC=g++ CXX=g++ LD=g++ make all-cc-tests
make run-cc-tests

# Do it all test execution.
make clean && CC=g++ CXX=g++ LD=g++ make all
make run-tests

echo " "
echo "${Me}: Run build-and-test for core L3 integration with LOC package and tests"
echo " "
set -x
make clean && CC=gcc LD=g++ L3_LOC_ENABLED=1 make all-c-tests
L3_LOC_ENABLED=1 make run-c-tests

test_l3_dump_py_missing_loc_decoder

make clean-l3 && CC=g++ CXX=g++ LD=g++ L3_LOC_ENABLED=1 make all-cpp-tests
L3_LOC_ENABLED=1 make run-cpp-tests

make clean-l3 && CC=g++ CXX=g++ LD=g++ L3_LOC_ENABLED=1 make all-cc-tests
L3_LOC_ENABLED=1 make run-cc-tests
