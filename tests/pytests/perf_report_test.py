# #############################################################################
# perf_report_test.py
#
"""
Small set of unit tests to verify output from parsing helper-methods that feed-into
perf_report_test.py, to generate the performance comparison report.
"""
import os
import sys

# #############################################################################
# Setup some variables pointing to diff dir/sub-dir full-paths.
# Dir-tree:
#  /tests/pytests/
#   - <this-file>
# Full dir-path where this tests/  dir lives
L3PytestsDir    = os.path.realpath(os.path.dirname(__file__))
L3RootDir       = os.path.realpath(L3PytestsDir + '/../..')
L3ScriptsDir    = L3RootDir + '/scripts/'

sys.path.append(L3ScriptsDir)

# pylint: disable-msg=import-error,wrong-import-position
import perf_report

# Canonical string generated from perf u-benchmarking scripts, used
# to test out parsing methods.
# pylint: disable=line-too-long
MSG1 = 'Baseline - No logging, NumClients=5, NumOps=5000000 (5 Million), NumThreads=1, Server throughput=91795 (~91.79 K) ops/sec, Client throughput=20938 (~20.93 K) ops/sec, elapsed_ns=54469084570 (~54.46 Billion) ns, NumOps/thread=5000 (5 K)'

# String to test parsing out for a line with same format but different field-names
# pylint: disable=line-too-long
MSG2 = 'Field is unrelated to parsing, Number-of-Xacts=5, Number-of-Conns=998877 (some-text ), Num-of-Threads=20, Metric-1 name=98602 (~98.60 K) ops/sec, Metric-2 name throughput=20938 (~20.93 K) ops/sec, elapsed_ns=54469084570 ns, nXacts/user=2500 ()'

# Strings to test out parsing done by split_field_value_str()
FIELD1 = 'Server throughput=53856 (~53.85 K) ops/sec'
FIELD2 = 'Server throughput=989 () ops/sec'

# #############################################################################
# To see output from test-cases run:
# $ pytest --capture=tee-sys perf_report_test.py -k test_compute_pct_drop
# #############################################################################

# #############################################################################
def test_parse_perf_line_names():
    """Verify parsing and output from parse_perf_line_names()"""

    (nclients_field, nops_field, nthreads_field, svr_metric, cli_metric, thread_metric) = perf_report.parse_perf_line_names(MSG1)

    assert nclients_field == 'NumClients=5'
    assert nops_field == 'NumOps=5000000 (5 Million)'
    assert nthreads_field == 'NumThreads=1'
    assert svr_metric == 'Server throughput'
    assert cli_metric == 'Client throughput'
    assert thread_metric == 'NumOps/thread'

    (nclients_field, nops_field, nthreads_field, svr_metric, cli_metric, thread_metric) = perf_report.parse_perf_line_names(MSG2)

    assert nclients_field == 'Number-of-Xacts=5'
    assert nops_field == 'Number-of-Conns=998877 (some-text )'
    assert nthreads_field == 'Num-of-Threads=20'
    assert svr_metric == 'Metric-1 name'
    assert cli_metric == 'Metric-2 name throughput'
    assert thread_metric == 'nXacts/user'

# #############################################################################
def test_parse_perf_line_values():
    """Verify parsing and output from parse_perf_line_values()"""

    (run_type, svr_value, svr_str, cli_value, cli_str, thread_str) = perf_report.parse_perf_line_values(MSG1)

    assert run_type == 'Baseline - No logging'
    assert svr_value == 91795
    assert svr_str == '~91.79 K ops/sec'
    assert cli_value == 20938
    assert cli_str == '~20.93 K ops/sec'
    assert thread_str == '5 K'

    (run_type, svr_value, svr_str, cli_value, cli_str, thread_str) = perf_report.parse_perf_line_values(MSG2)

    assert run_type == 'Field is unrelated to parsing'
    assert svr_value == 98602
    assert svr_str == '~98.60 K ops/sec'
    assert cli_value == 20938
    assert cli_str == '~20.93 K ops/sec'
    assert thread_str == '2500'

# #############################################################################
def test_split_field_value_str():
    """Verify parsing and output from split_field_value_str()"""

    (f_value, f_str) = perf_report.split_field_value_str(FIELD1)
    assert f_value == 53856
    assert f_str == '~53.85 K ops/sec'

    (f_value, f_str) = perf_report.split_field_value_str(FIELD2)
    assert f_value == 989
    assert f_str == '989 ops/sec'


# #############################################################################
def test_compute_pct_drop():
    """
    Verify %age-drop computed by compute_pct_drop(). Hopefully, the values
    chosen and returned %-age drops won't vary on diff test machines due to
    float-computation precision.
    """

    pct_float = perf_report.compute_pct_drop(100, 105)
    assert pct_float == 5.0

    baseval = 100000
    newval = 100000 + 230
    pct_float = perf_report.compute_pct_drop(baseval, newval)
    assert pct_float == 0.23

    one_k = 1000
    baseval = 91.79 * one_k
    newval = 101.81 * one_k
    pct_float = perf_report.compute_pct_drop(baseval, newval)
    assert pct_float == 10.92
