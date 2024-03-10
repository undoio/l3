#!/bin/bash
# ##############################################################################
# test.sh - Driver script to build-and-test L3 toolset.
# This script issues the same kinds of commands that are executed in
# .github/workflows/build.yml . So, yes, there is some duplication, but this
# script is provided for developers to test the changes before invoking CI.
# ##############################################################################

Me=$(basename "$0")
set -euo pipefail

# Currently, this is a simplistic driver, just executing basic `make` commands
# to ensure that all code / tools build correctly in release mode.

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
