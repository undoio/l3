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

UNAME_S=$(uname -s)

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
# Global variables to support different workloads / configurations to
# execute client-server performance u-benchmarking tests.
# ##################################################################
# Number of messages sent by each client to the server
NumMsgsPerClient=1000

# Number of clients started for client-server RPC-perf testing.
NumClients=5

# Server-clock cmdline arg. Default is CLOCK_REALTIME clock
SvrClockArg=

# Server --perf-outfile arg. Default is none. Used by test-method
# that runs all client-server perf benchmarking tests
SvrPerfOutfileArg=

# Server can be run with the '--num-server-threads <n>' argument, which
# currently is restricted to 1-thread. In a future check-in, we will be
# able to specify a list of server-threads to execute, using env-var as:
#
#   SERVER_NUM_THREADS="1 2 4 8" ./test.sh run-all-client-server-perf-tests
#
# SrvNumThreadsList="${SERVER_NUM_THREADS:-1}"
SrvNumThreadsList="1"

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

           "test-build-and-run-sample-c-appln"
           "test-build-and-run-sample-c-appln-with-LOC"
           "test-build-and-run-sample-c-appln-with-LOC-ELF"

           # For dev-usage; not invoked thru CI.
           "run-all-client-server-perf-tests"

           # Collection of individual client-server performance test-methods
           # each invoking different logging schemes, for perf u-benchmarking.
           "test-build-and-run-csperf-baseline"
           "test-build-and-run-csperf-l3-log"
           "test-build-and-run-csperf-fast-log-no-LOC"

           "test-build-and-run-csperf-fprintf"
           "test-build-and-run-csperf-write"
           "test-build-and-run-csperf-l3_loc_eq_1"
           "test-build-and-run-csperf-l3_loc_eq_2"
           "test-build-and-run-csperf-spdlog"
           "test-build-and-run-csperf-spdlog-backtrace"

           "test-pytests"

           # spdlog-related test methods
           "test-build-and-run-spdlog-sample"

           # Driver methods, listed here for quick debugging.
           # Not intended for use by test-execution.
           "run-client-server-tests_vary_threads"

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

   echo " "
   echo "Run client-server performance tests as follows:"
   echo " "
   echo " ${Me} run-all-client-server-perf-tests [ server-clock-ID [ num-msgs [ num-clients ] ] ]"
   echo " "
   echo " ${Me} test-build-and-run-csperf [ server-clock-ID [ num-msgs ] ]"
   echo " "
   echo " ${Me} test-build-and-run-csperf-l3_loc_eq_1 [ server-clock-ID [ num-msgs ] ]"
   echo " "
   echo " ${Me} test-build-and-run-csperf-l3_loc_eq_2 [ server-clock-ID [ num-msgs ] ]"
   echo " "
   echo "  Client-Server performance u-benchmarking defaults:"
   echo "    Number of clients                      = ${NumClients}"
   echo "    Number of messages sent by each client = ${NumMsgsPerClient}"
   echo "    Default clock-ID used (client)         = CLOCK_REALTIME"
   echo "    Default clock-ID used (server)         = CLOCK_REALTIME"
   echo "    Alternate server clock-selector: See \`svmsg_file_server --help\`"
   echo "       One of: --clock-default | --clock-monotonic | --clock-process-cputime-id | --clock-realtime | --clock-thread-cputime-id"
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
function test-build-and-run-sample-c-appln()
{
    # Use default L3-logging with no LOC-encoding
    build-and-run-sample-c-appln-with-LOC-encoding
}

# #############################################################################
function test-build-and-run-sample-c-appln-with-LOC()
{
    build-and-run-sample-c-appln-with-LOC-encoding 1
}

# #############################################################################
function test-build-and-run-sample-c-appln-with-LOC-ELF()
{
    build-and-run-sample-c-appln-with-LOC-encoding 2
}

# #############################################################################
# Common build-and-run method to `make c-sample` and run it.
# Although the name says "with-LOC-encoding", default invocation is without
# any argument, so we do default build with just L3-logging.
# #############################################################################
function build-and-run-sample-c-appln-with-LOC-encoding()
{
    local loc_encoding=
    if [ $# -eq 1 ]; then
        loc_encoding=$1
    fi

    local l3_dump
    l3_dump="$(pwd)/l3_dump.py"

    pushd ./samples/sample-c-appln/ > /dev/null 2>&1

    local program="./c-sample"

    L3_LOC_ENABLED=${loc_encoding} make clean
    L3_LOC_ENABLED=${loc_encoding} make c-sample

    set +x

    echo " "
    echo "${Me}: Dump the log-file generated by ${program}"
    echo " "
    set -x

    ${l3_dump} --log-file /tmp/c-sample-test.dat --binary ${program}

    popd > /dev/null 2>&1
}

# #############################################################################
# Driver method to run through all variations of client-server perf tests
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
#   $3  - (Opt) # of clients to start-up (default: 5)
#
# Usage:
#  $ ./test.sh run-all-client-server-perf-tests --clock-default $((100 * 1000)) > /tmp/perf-test.100K-rows.5ct.out 2>&1
#
#  $ egrep -B1 -E 'Start Server|, num_ops=|Unpacked nentries|throughput=' /tmp/perf-test.100K-rows.5ct.out | egrep -v -E 'Client-throughput|enabled on server-side.'

# #############################################################################
function run-all-client-server-perf-tests()
{
    set +x
    if [ "${UNAME_S}" = "Darwin" ]; then
        echo "${Me}: Client-server performance tests not supported currently on Mac/OSX."
        return
    fi

    set -x
    # Reset global if arg-1 provided. Minions will grab it from there.
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    if [ $# -ge 3 ]; then
        NumClients=$3
    fi

    set +x
    local outfile="/tmp/${Me}.${FUNCNAME[0]}.out"
    echo "${Me} tail -f $outfile"
    echo " "
    if [ -f "${outfile}" ]; then rm -rf "${outfile}"; fi

    # Reset global, so it will be picked up when server binary is started.
    SvrPerfOutfileArg="--perf-outfile ${outfile}"

    set -x
    local start_seconds=$SECONDS

    echo "${Me}: $(TZ="America/Los_Angeles" date) Run all client(s)-server communication test."
    echo " "
    test-build-and-run-csperf-baseline "${SvrClockArg}" "${num_msgs_per_client}"

    test-build-and-run-csperf-l3-log "${SvrClockArg}" "${num_msgs_per_client}"

    test-build-and-run-csperf-fast-log-no-LOC "${SvrClockArg}" "${num_msgs_per_client}"

    test-build-and-run-csperf-fprintf "${SvrClockArg}" "${num_msgs_per_client}"

    test-build-and-run-csperf-write "${SvrClockArg}" "${num_msgs_per_client}"

    test-build-and-run-csperf-l3_loc_eq_1 "${SvrClockArg}" "${num_msgs_per_client}"

    test-build-and-run-csperf-l3_loc_eq_2 "${SvrClockArg}" "${num_msgs_per_client}"

    test-build-and-run-csperf-spdlog "${SvrClockArg}" "${num_msgs_per_client}"

    test-build-and-run-csperf-spdlog-backtrace "${SvrClockArg}" "${num_msgs_per_client}"

    echo " "
    set -x
    ./scripts/perf_report.py --file "${outfile}"
    set +x

    local total_seconds=$((SECONDS - start_seconds))
    el_h=$((total_seconds / 3600))
    el_m=$((total_seconds % 3600 / 60))
    el_s=$((total_seconds % 60))
    echo " "
    echo "${Me}: $(TZ="America/Los_Angeles" date) Completed all client(s)-server communication test [ ${el_h}h ${el_m}m ${el_s}s ]."
    echo " "
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark.
# This test-case runs with no-L3-logging (baseline).
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-baseline()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    local l3_log_disabled=0

    set +x
    echo " "
    echo "**** ${Me}: Client-server performance testing with L3-logging OFF ${SvrClockArg}:"
    echo " "
    set -x

    build-client-server-programs "${l3_log_disabled}"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_disabled}"

    local server_bin="./build/${Build_mode}/bin/use-cases/svmsg_file_server"
    local client_bin="./build/${Build_mode}/bin/use-cases/svmsg_file_client"

    ${client_bin} --help

    ${server_bin} --help
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark.
# This test-case runs with L3-logging enabled.
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-l3-log()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    local l3_log_enabled=1

    echo " "
    echo "**** ${Me}: Client-server performance testing with L3-logging ON ${SvrClockArg}:"
    echo " "

    build-client-server-programs "${l3_log_enabled}"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_enabled}"

    # Perf-tests are only executed on Linux. So, skip L3-dump for non-Linux p/fs.
    if [ "${UNAME_S}" == "Linux" ]; then
        local nentries=100
        echo " "
        echo "${Me}: Run L3-dump script to unpack log-entries. (Last ${nentries} entries.)"
        echo " "

        set -x
        # No LOC-encoding is in-effect, so no need for the --loc-binary argument.
        ./l3_dump.py                                                                \
                --log-file /tmp/l3.c-server-test.dat                                \
                --binary "./build/${Build_mode}/bin/use-cases/svmsg_file_server"    \
            | tail -${nentries}
    fi
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark.
# This test-case runs with no-L3-logging and L3-logging enabled.
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-fast-log-no-LOC()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    local l3_log_enabled=1
    local l3_LOC_disabled=0

    echo " "
    echo "${Me}: Client-server performance testing with L3-fast-logging ON ${SvrClockArg}:"
    echo " "

    build-client-server-programs "${l3_log_enabled}"       \
                                 "${l3_LOC_disabled}"      \
                                 "fast"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_enabled}"

    # Perf-tests are only executed on Linux. So, skip L3-dump for non-Linux p/fs.
    if [ "${UNAME_S}" == "Linux" ]; then
        local nentries=100
        echo " "
        echo "${Me}: Run L3-dump script to unpack log-entries. (Last ${nentries} entries.)"
        echo " "

        set -x
        # No LOC-encoding is in-effect, so no need for the --loc-binary argument.
        ./l3_dump.py                                                                \
                --log-file /tmp/l3.c-server-test.dat                                \
                --binary "./build/${Build_mode}/bin/use-cases/svmsg_file_server"    \
            | tail -${nentries}
    fi
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark, using the
# fprintf() logging interface under L3..
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-fprintf()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi


    local l3_log_enabled=1
    local l3_LOC_disabled=0

    echo " "
    echo "**** ${Me}: Client-server performance testing with L3-fprintf logging ON:"
    echo " "

    build-client-server-programs "${l3_log_enabled}"        \
                                 "${l3_LOC_disabled}"       \
                                 "fprintf"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_enabled}"
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark, using the
# write() msg logging interface under L3.
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-write()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    local l3_log_enabled=1
    local l3_LOC_disabled=0

    echo " "
    echo "**** ${Me}: Client-server performance testing with L3-write logging ON:"
    echo " "

    build-client-server-programs "${l3_log_enabled}"        \
                                 "${l3_LOC_disabled}"       \
                                 "write"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_enabled}"
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark.
# This test-case runs with L3-logging and L3-LOC (default) scheme enabled.
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-l3_loc_eq_1()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    local l3_log_enabled=1
    local l3_LOC_enabled=1

    echo " "
    echo "${Me}: Client-server performance testing with L3-logging and L3-LOC ON ${SvrClockArg}:"
    echo " "

    build-client-server-programs "${l3_log_enabled}" "${l3_LOC_enabled}"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_enabled}"

    # Perf-tests are only executed on Linux. So, skip L3-dump for non-Linux p/fs.
    if [ "${UNAME_S}" == "Linux" ]; then
        local nentries=100
        echo " "
        echo "${Me}: Run L3-dump script to unpack log-entries. (Last ${nentries} entries.)"
        echo " "
        set -x
        L3_LOC_ENABLED=1 ./l3_dump.py                                   \
                            --log-file /tmp/l3.c-server-test.dat        \
                            --binary "./build/${Build_mode}/bin/use-cases/svmsg_file_server"    \
                            --loc-binary "./build/${Build_mode}/bin/use-cases/client-server-msgs-perf_loc" \
                    | tail -${nentries}
    fi

    set +x
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark.
# This test-case runs with L3-logging and L3-ELF scheme enabled.
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-l3_loc_eq_2()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    local l3_log_enabled=1
    local l3_LOC_enabled=2

    echo " "
    echo "${Me}: Client-server performance testing with L3-logging and LOC-ELF ON ${SvrClockArg}:"
    echo " "

    build-client-server-programs "${l3_log_enabled}" "${l3_LOC_enabled}"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_enabled}"

    # Perf-tests are only executed on Linux. So, skip L3-dump for non-Linux p/fs.
    if [ "${UNAME_S}" == "Linux" ]; then
        local nentries=100
        echo " "
        echo "${Me}: Run L3-dump script to unpack log-entries. (Last ${nentries} entries.)"
        echo " "

        # Currently, we are not unpacking LOC-ELF-IDs, so, for dumping this kind
        # of logging, we don't need the --loc-binary argument.
        L3_LOC_ENABLED=2 ./l3_dump.py                                   \
                            --log-file /tmp/l3.c-server-test.dat        \
                            --binary "./build/${Build_mode}/bin/use-cases/svmsg_file_server"    \
                    | tail -${nentries}
    fi
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark,
# with C++ spdlog logging.
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-spdlog()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    set +x

    # spdlog is used here only for performance benchmarking.
    local l3_log_disabled=0
    local l3_LOC_disabled=0

    echo " "
    echo "${Me}: Client-server performance testing with spdlog-logging:"
    echo " "

    build-client-server-programs "${l3_log_disabled}"   \
                                 "${l3_LOC_disabled}"   \
                                 "spdlog"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_disabled}"

    # Perf-tests are only executed on Linux. So, skip L3-dump for non-Linux p/fs.
    if [ "${UNAME_S}" == "Linux" ]; then
        local nentries=100
        echo " "
        tail -${nentries} /tmp/l3.c-server-test.dat
    fi
}

# #############################################################################
# Test build-and-run of client-server performance test benchmark,
# with C++ spdlog backtrace logging.
#
# Parameters:
#   $1  - (Opt) Arg to select clock-ID to use
#   $2  - (Opt) # of messages to exchange from client -> server (default: 1000)
# #############################################################################
function test-build-and-run-csperf-spdlog-backtrace()
{
    if [ $# -ge 1 ]; then
        SvrClockArg=$1
    fi

    local num_msgs_per_client=${NumMsgsPerClient}
    if [ $# -ge 2 ]; then
        num_msgs_per_client=$2
    fi

    set +x

    # spdlog is used here only for performance benchmarking.
    local l3_log_disabled=0
    local l3_LOC_disabled=0

    echo " "
    echo "${Me}: Client-server performance testing with spdlog-backtrace logging:"
    echo " "

    build-client-server-programs "${l3_log_disabled}"   \
                                 "${l3_LOC_disabled}"   \
                                 "spdlog-backtrace"

    run-client-server-tests_vary_threads "${num_msgs_per_client}" "${l3_log_disabled}"
}

# #############################################################################
# Test-method to build the client-server programs with required build flags.
#
# Parameters:
#   $1  - (Reqd) Boolean: Is L3 enabled?
#   $2  - (Opt.) When L3 is enabled, type of L3-LOC encoding enabled
#   $3  - (Opt.) L3-logging type: 'slow' (default), 'fast'
# #############################################################################
function build-client-server-programs()
{
    local l3_enabled=$1

    local l3_loc_enabled=
    if [ $# -ge 2 ]; then
        l3_loc_enabled=$2
    fi

    local l3_log_type=
    if [ $# -ge 3 ]; then
        l3_log_type=$3
    fi

    echo "${Me}: Build: l3_enabled='${l3_enabled}', l3_loc_enabled='${l3_loc_enabled}', l3_log_type='${l3_log_type}'"
    set +x

    # Default L3-intrinsic logging execution modes
    if [ "${l3_log_type}" == "" ]; then
        set -x
        make clean                              \
        && CC=gcc LD=g++                        \
           L3_ENABLED=${l3_enabled}             \
           L3_LOC_ENABLED=${l3_loc_enabled}     \
           BUILD_VERBOSE=1                      \
           make client-server-perf-test
    else

        # Execute the specified logging scheme under L3-logging APIs
        case "${l3_log_type}" in

            "fast")
                set -x
                make clean                          \
                && CC=gcc LD=g++                    \
                    L3_ENABLED=${l3_enabled}        \
                    L3_FASTLOG_ENABLED=1            \
                    BUILD_VERBOSE=1                 \
                    make client-server-perf-test
                ;;

            "fprintf")
                set -x
                make clean                      \
                && CC=gcc LD=g++               \
                    L3_ENABLED=${l3_enabled}    \
                    BUILD_VERBOSE=1             \
                    L3_LOGT_FPRINTF=1           \
                    make client-server-perf-test
                ;;

            "write")
                set -x
                make clean                      \
                && CC=gcc LD=g++               \
                    L3_ENABLED=${l3_enabled}    \
                    BUILD_VERBOSE=1             \
                    L3_LOGT_WRITE=1             \
                    make client-server-perf-test
                ;;

            "spdlog")
                set -x
                make clean                          \
                && CC=g++ LD=g++                    \
                    L3_ENABLED=${l3_enabled}        \
                    L3_LOGT_SPDLOG=1                \
                    BUILD_VERBOSE=1                 \
                    make client-server-perf-test
                ;;

            "spdlog-backtrace")
                set -x
                make clean                          \
                && CC=g++ LD=g++                    \
                    L3_ENABLED=${l3_enabled}        \
                    L3_LOGT_SPDLOG=2                \
                    BUILD_VERBOSE=1                 \
                    make client-server-perf-test
                ;;

            *)
                echo "${Me}: Unknown L3-logging type '${l3_log_type}'. Exiting."
                exit 1
                ;;
        esac
    fi
}

# #############################################################################
# Test-method to run the client-server programs with required exec parameters,
# This method will run multiple executions of the client-server test with
# different server-thread counts as obtained from the SrvNumThreadsList globals.
#
# Parameters:
#   $1  - (Reqd) # of messages to exchange from client -> server
#   $2  - (Reqd) Boolean: Is L3 enabled?
# #############################################################################
function run-client-server-tests_vary_threads()
{
    local num_msgs_per_client=$1
    local l3_enabled=$2

    set +x

    for threads in ${SrvNumThreadsList}; do
        echo " "
        echo "${Me}:${LINENO}: ***** Run run-client-server-test with l3_enabled=${l3_enabled}, num_msgs_per_client=${num_msgs_per_client}, ${threads} threads ..."
        echo " "

        run-client-server-test "${num_msgs_per_client}" "${l3_enabled}"

    done

    set -x
}

# #############################################################################
# Test-method to run the client-server programs with required exec parameters.
#
# Parameters:
#   $1  - (Reqd) # of messages to exchange from client -> server
#   $2  - (Reqd) Boolean: Is L3 enabled?
# #############################################################################
function run-client-server-test()
{
    local num_msgs_per_client=$1
    local l3_enabled=$2

    local num_server_threads=1
    if [ $# -ge 3 ]; then
        num_server_threads=$3
    fi

    set +x
    # Makefile does not implement 'run' step. Do it here manually.
    local server_bin="./build/${Build_mode}/bin/use-cases/svmsg_file_server"
    local client_bin="./build/${Build_mode}/bin/use-cases/svmsg_file_client"

    set +x
    if [ "${UNAME_S}" = "Darwin" ]; then
        echo "${Me}: Execution of client-server performance tests not supported currently on Mac/OSX."
        return
    fi

    set +x
    echo " "
    echo "${Me}: $(TZ="America/Los_Angeles" date) Started basic client(s)-server communication test."
    echo " "

    # User can execute perf-bm tests with different server-clocks,
    # using the --clock-<...> argument.
    # shellcheck disable=SC2086
    ${server_bin} ${SvrClockArg} ${SvrPerfOutfileArg}           \
                  --num-server-threads ${num_server_threads} &

    set +x
    sleep 5

    local numclients=${NumClients}
    local ictr=0
    # shellcheck disable=SC2086
    while [ ${ictr} -lt ${numclients} ]; do

        set -x
        ${client_bin} "${num_msgs_per_client}" &
        set +x

        ictr=$((ictr + 1))
    done

    wait

    set +x
    echo " "
    echo "${Me}: $(TZ="America/Los_Angeles" date) Completed basic client(s)-server communication test."
    echo " "

    # Report the log-file size, if generated, so we know how big it could get.
    if [ "${l3_enabled}" = "1" ]; then

        set -x

        du -sh /tmp/l3.c-server-test.dat

        set +x
    fi
}

# #############################################################################
function test-pytests()
{
    pushd ./tests/pytests

    pytest -v

    popd
}

# #############################################################################
# Build-and-run spdlog sample tutorial program. This test-method validates that
# the required dependencies for using spdlog, integrated with L3, are met.
# #############################################################################
function test-build-and-run-spdlog-sample()
{
    make clean && CC=g++ LD=g++ BUILD_VERBOSE=1 make spdlog-cpp-program

    local test_prog="./build/${Build_mode}/bin/use-cases/spdlog-Cpp-program"

    set +x
    echo " "
    echo "${Me}: Run spdlog sample program ..."
    echo " "

    set -x
    ${test_prog}
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
    shift
    set -x
    # shellcheck disable=SC2048,SC2086
    ${test_fnname} $*
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

    # Some test-methods accept args from command-line. Pass-them along using $*
    This_fn=$1
    # shellcheck disable=SC2048,SC2086
    run_test $*
    exit 0
fi

test_all ""
exit 0
