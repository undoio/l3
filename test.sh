#!/bin/bash
# ##############################################################################
# test.sh - Driver script to build-and-test L3 toolset.
# This script provides "test" methods that are also invoked as part of CI
# .github/workflows/build.yml . This way, if you run $ ./test.sh
# you should get the same build-and-test coverage executed by CI jobs.
#
# History:
# Modelled on a similar script from the Certifier Framework library:
# See: https://github.com/ccc-certifier-framework/certifier-framework-for-confidential-computing/blob/main/CI/scripts/test.sh
# ##############################################################################

Me=$(basename "$0")
set -Eeuo pipefail

Build_mode="${BUILD_MODE:-release}"

# 'Globals' to track which test function is / was executing ...
This_fn=""

# Arguments supported by L3 dump utility
L3_LOG_FILE="--log-file"
L3_BINARY="--binary"

# ###########################################################################
# Set trap handlers for all errors. Needs -E (-o errtrace): Ensures that ERR
# traps (see below) get inherited by functions, command substitutions, and
# subshell environments
# Ref: https://citizen428.net/blog/bash-error-handling-with-trap/
# ###########################################################################
function cleanup()
{
    set +x
    echo " "
    echo "${Me}: **** Error **** Failed command, ${BASH_COMMAND}, at line $(caller) while executing function ${This_fn}"
}

trap cleanup ERR

# ##################################################################
# Array of test function names. If you add a new test_<function>,
# add it to this list, here, so that one can see it in --list output.
# ##################################################################
TestList=(
           "test-build-and-run-unit-tests"

           "test-build-C-samples"
           "test-run-C-samples"

           "test-build-Cpp-samples"
           "test-run-Cpp-samples"

           "test-build-Cc-samples"
           "test-run-Cc-samples"

           # These two should go back-to-back as a pair
           "test-build-and-run-C-samples-with-LOC"
           "test-run-C-samples-with-LOC-wo-L3_LOC_ENABLED"

           "test-l3_dump_py-missing-loc_decoder"

           # These two should go back-to-back as a pair
           "test-build-and-run-Cpp-samples-with-LOC"
           "test-run-Cpp-samples-with-LOC-wo-L3_LOC_ENABLED"

           # These two should go back-to-back as a pair
           "test-build-and-run-Cc-samples-with-LOC"
           "test-run-Cc-samples-with-LOC-wo-L3_LOC_ENABLED"

           "test-build-and-run-LOC-unit-test"

           "test-build-and-run-C-samples-with-LOC-ELF"
           "test-build-and-run-Cpp-samples-with-LOC-ELF"
           "test-build-and-run-Cc-samples-with-LOC-ELF"

           "test-build-and-run-client-server-perf-test"
           "test-build-and-run-client-server-perf-test-l3_loc_eq_1"

           "test-pytests"

           # Keep these two at the end, so that we can exercise this
           # script in CI with the --from-test interface, to run just
           # these two tests.
           "test-pylint-check"
           "test-make-help"
        )

# #############################################################################
# Print help / usage
function usage()
{
   echo "Usage: $Me [--help | --list] [ --from-test <test-function-name> ] [test_all]"
}

# --------------------------------------------------------------------------
# Driver function to list test functions available.
function list_tests()
{
    echo "${Me}: List of builds and test cases to execute:"
    list_items_for_array "${TestList[@]}"
}

function list_items_for_array()
{
    local items_array=("$@")
    for str in "${items_array[@]}"; do
        echo "  ${str}"
    done
}

# #############################################################################
# List the test-function in the same order here as listed in the TestList[].
# (The 1st two are exceptions, left here for readability.)
# #############################################################################
function test-pylint-check()
{
    pylint ./l3_dump.py
    pylint ./tests/pytests
}

# #############################################################################
function test-make-help()
{
    make help
}

# #############################################################################
function test-build-and-run-unit-tests()
{
    make clean
    CC=gcc LD=g++ make run-unit-tests
}

# #############################################################################
function test-build-C-samples()
{
    make clean
    CC=gcc LD=g++ make all-c-tests
}

# #############################################################################
function test-run-C-samples()
{
    make run-c-tests
}

# #############################################################################
function test-build-Cpp-samples()
{
    make clean
    CC=g++ CXX=g++ LD=g++ make all-cpp-tests
}

# #############################################################################
function test-run-Cpp-samples()
{
    make run-cpp-tests
}

# #############################################################################
function test-build-Cc-samples()
{
    make clean
    CC=g++ CXX=g++ LD=g++ make all-cc-tests
}

# #############################################################################
function test-run-Cc-samples()
{
    make run-cc-tests
}

# #############################################################################
function test-build-and-run-C-samples-with-LOC()
{
    make clean
    CC=gcc LD=g++ L3_LOC_ENABLED=1 make run-c-tests
}

# #############################################################################
# Just re-run the tests on data that was generated with LOC-enabled, but
# without the required env-var. This should exercise code in the Python
# dump script to silently skip decoding LOC-ID values.
# You need to execute this run tests target immediately after doing the
# build; otherwise, other test execution commands will `clean` stuff
# which will cause this run to need re-builds.
# #############################################################################
function test-run-C-samples-with-LOC-wo-L3_LOC_ENABLED()
{
    CC=gcc LD=g++ make run-c-tests
}

# #############################################################################
# Error-checking test-case: This should come immediately after
# test-run-C-samples-with-LOC-wo-L3_LOC_ENABLED(), so the required LOC-binary
# is already present.
#
# Exercise the check in ./l3_dump.py which verifies that the LOC-decoder
# binary exists for LOC-encoding output data. If not, it should fail with an
# error message.
# #############################################################################
function test-l3_dump_py-missing-loc_decoder()
{
    # Turn OFF, so test execution continues w/o failing immediately
    set +e

    # This binary should have been generated by previous test-case,
    # test-run-C-samples-with-LOC-wo-L3_LOC_ENABLED()
    local test_prog="./build/${Build_mode}/bin/use-cases/single-file-C-program"

    # Python dump script expects this LOC-decoder binary, with _loc suffix.
    local loc_prog="${test_prog}_loc"
    local tmp_prog="${loc_prog}.bak"
    set -x
    mv "${loc_prog}" "${tmp_prog}"

    L3_LOC_ENABLED=1 ./l3_dump.py "${L3_LOG_FILE}" /tmp/l3.c-small-test.dat \
                                  "${L3_BINARY}" "${test_prog}"

    mv "${tmp_prog}" "${loc_prog}"
    set +x
    set -e
    echo " "
    echo "${Me}: Expected failure in ./l3_dump.py execution. Continuiing ..."
    echo " "
}

# #############################################################################
function test-build-and-run-Cpp-samples-with-LOC()
{
    make clean
    CC=g++ CXX=g++ LD=g++ L3_LOC_ENABLED=1 make run-cpp-tests
}

# #############################################################################
function test-run-Cpp-samples-with-LOC-wo-L3_LOC_ENABLED()
{
    CC=g++ CXX=g++ LD=g++ make run-cpp-tests
}

# #############################################################################
function test-build-and-run-Cc-samples-with-LOC()
{
    make clean
    CC=g++ CXX=g++ LD=g++ L3_LOC_ENABLED=1 make run-cc-tests
}

# #############################################################################
function test-run-Cc-samples-with-LOC-wo-L3_LOC_ENABLED()
{
    make clean
    CC=g++ CXX=g++ LD=g++ make run-cc-tests
}

# #############################################################################
function test-build-and-run-LOC-unit-test()
{
    make clean
    CC=g++ CXX=g++ LD=g++ BUILD_VERBOSE=1 L3_LOC_ENABLED=2 make run-loc-tests
}

# #############################################################################
# Run these builds with verbose output so we can see that the loc.c file is
# being compiled with gcc even when CC is specified as g++ (or some other non-gcc)
function test-build-and-run-C-samples-with-LOC-ELF()
{
    make clean
    CC=gcc LD=g++ BUILD_VERBOSE=1 L3_LOC_ENABLED=2 make run-c-tests
}

# #############################################################################
function test-build-and-run-Cpp-samples-with-LOC-ELF()
{
    make clean
    CC=g++ CXX=g++ LD=g++ BUILD_VERBOSE=1 L3_LOC_ENABLED=2 make run-cpp-tests
}

# #############################################################################
function test-build-and-run-Cc-samples-with-LOC-ELF()
{
    make clean
    CC=g++ CXX=g++ LD=g++ BUILD_VERBOSE=1 L3_LOC_ENABLED=2 make run-cc-tests
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark.
# This test-case runs with no-L3-logging and L3-logging enabled.
# #############################################################################
function test-build-and-run-client-server-perf-test()
{
    set +x

    echo " "
    echo "${Me}: Client-server performance testing with L3-logging OFF:"
    echo " "
    build-and-run-client-server-perf-test 0

    echo " "
    echo "${Me}: Client-server performance testing with L3-logging ON:"
    echo " "
    build-and-run-client-server-perf-test 1

    echo " "
    echo "${Me}: Completed basic client(s)-server communication test."
    echo " "
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark.
# This test-case runs with L3-logging and L3-LOC (default) scheme enabled.
# #############################################################################
function test-build-and-run-client-server-perf-test-l3_loc_eq_1()
{
    set +x

    echo " "
    echo "${Me}: Client-server performance testing with L3-logging and L3-LOC ON:"
    echo " "
    build-and-run-client-server-perf-test 1 1

    local nentries=100
    echo " "
    echo "${Me}: Run L3-dump script to unpack log-entries. (Last ${nentries} entries.)"
    echo " "
    L3_LOC_ENABLED=1 ./l3_dump.py                                                           \
                        --log-file /tmp/l3.c-server-test.dat                                \
                        --binary "./build/${Build_mode}/bin/use-cases/svmsg_file_server"    \
                        --loc-binary "./build/${Build_mode}/bin/use-cases/client-server-msgs-perf_loc" \
                | tail -${nentries}

    echo " "
    echo "${Me}: Completed basic client(s)-server communication test."
    echo " "
}

# #############################################################################
# Minion to test-build-and-run-client-server-perf-test(), to actually perform
# the build and run the client/server application for performance benchmarking.
# #############################################################################
function build-and-run-client-server-perf-test()
{
    local l3_enabled=$1
    local l3_loc_enabled=
    if [ $# -eq 2 ]; then
        l3_loc_enabled=$2
    fi

    set +x
    # Makefile does not implement 'run' step. Do it here manually.
    local server_bin="./build/${Build_mode}/bin/use-cases/svmsg_file_server"
    local client_bin="./build/${Build_mode}/bin/use-cases/svmsg_file_client"

    set -x
    make clean \
    && CXX=g++ LD=g++ L3_ENABLED=${l3_enabled} L3_LOC_ENABLED=${l3_loc_enabled} make client-server-perf-test

    ${server_bin} &

    set +x
    sleep 5

    local numclients=5
    local ictr=0
    while [ ${ictr} -lt ${numclients} ]; do

        set -x
        ${client_bin} 1000 &
        set +x

        ictr=$((ictr + 1))
    done

    wait
}

# #############################################################################
function test-pytests()
{
    pushd ./tests/pytests

    pytest -v

    popd
}

# #############################################################################
# Driver function to run a specific named test-method.
# #############################################################################
function run_test()
{
    test_fnname=$1
    echo " "
    echo "-------------------------------------------------------------------------"
    echo "${Me}: $(TZ="America/Los_Angeles" date) Executing ${test_fnname} ..."
    echo " "
    set -x
    ${test_fnname}
    set +x
}

# #############################################################################
# Run through the list of test-cases in TestList[] and execute each one.
# #############################################################################
function test_all()
{
    from_test=$1
    msg="L3 build-and-test execution"
    if [ "${from_test}" = "" ]; then
        exec_msg="Start ${msg}"
    else
        exec_msg="Resume ${msg} from ${from_test}"
    fi

    echo "${Me}: $(TZ="America/Los_Angeles" date) ${exec_msg} ..."
    echo " "

    start_seconds=$SECONDS
    for str in "${TestList[@]}"; do

        # Skip tests if we are resuming test execution using --from-test
        if [ "${from_test}" != "" ] && [ "${from_test}" != "${str}" ]; then
            continue
        fi
        This_fn="${str}"
        # shellcheck disable=SC2086
        run_test $str

        from_test=""    # Reset, so we can continue w/next test.
    done

   # Computed elapsed hours, mins, seconds from total elapsed seconds
   total_seconds=$((SECONDS - start_seconds))
   el_h=$((total_seconds / 3600))
   el_m=$((total_seconds % 3600 / 60))
   el_s=$((total_seconds % 60))

   echo " "
   echo "${Me}: $(TZ="America/Los_Angeles" date) Completed ${msg}: ${total_seconds} s [ ${el_h}h ${el_m}m ${el_s}s ]"

}

# ##################################################################
# main() begins here
# ##################################################################

if [ $# -eq 1 ]; then
    if [ "$1" == "--help" ]; then
        usage
        exit 0
    elif [ "$1" == "--list" ]; then
        list_tests
        exit 0
    fi
fi

# ------------------------------------------------------------------------
# Fast-path execution support. You can invoke this script specifying the
# name of one of the functions to execute a specific set of tests.
# This way, one can debug script changes to ensure that test-execution
# still works.
# ------------------------------------------------------------------------
if [ $# -ge 1 ]; then

    if [ "$1" == "--from-test" ]; then
        if [ $# -eq 1 ]; then
            echo "${Me}: Error --from-test needs the name of a test-function to resume execution from."
            exit 1
        fi
        # Resume test execution from named test-function
        # shellcheck disable=SC2048,SC2086
        test_all $2
        exit 0
    fi

    This_fn=$1
    # shellcheck disable=SC2048,SC2086
    run_test $*
    exit 0
fi

test_all ""
exit 0

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
