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

# pylint: disable=line-too-long
SAMPLE_SUMM_LINE = 'Summary: For 32 clients, Baseline - No logging, num_threads=1, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'

# pylint: enable=line-too-long

# #############################################################################
# To see output from test-cases run:
# $ pytest --capture=tee-sys perf_report_test.py -k test_compute_pct_drop
# #############################################################################

# #############################################################################
def test_parse_perf_line_names():
    """Verify parsing and output from parse_perf_line_names()"""

    # pylint: disable-next=line-too-long
    (nclients_field, nops_field, nthreads_field, svr_metric, cli_metric, thread_metric) = perf_report.parse_perf_line_names(MSG1)

    assert nclients_field == 'NumClients=5'
    assert nops_field == 'NumOps=5000000 (5 Million)'
    assert nthreads_field == 'NumThreads=1'
    assert svr_metric == 'Server throughput'
    assert cli_metric == 'Client throughput'
    assert thread_metric == 'NumOps/thread'

    # pylint: disable-next=line-too-long
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

    # pylint: disable-next=line-too-long
    (run_type, nthreads_field, svr_value, svr_str, cli_value, cli_str, thread_str) = perf_report.parse_perf_line_values(MSG1)

    assert run_type == 'Baseline - No logging'
    assert nthreads_field == 'NumThreads=1'
    assert svr_value == 91795
    assert svr_str == '~91.79 K ops/sec'
    assert cli_value == 20938
    assert cli_str == '~20.93 K ops/sec'
    assert thread_str == '5 K'

    # pylint: disable-next=line-too-long
    (run_type, nthreads_field, svr_value, svr_str, cli_value, cli_str, thread_str) = perf_report.parse_perf_line_values(MSG2)

    assert run_type == 'Field is unrelated to parsing'
    assert nthreads_field == 'Num-of-Threads=20'
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

# #############################################################################
def test_extract_nth_field():
    """
    Verify extraction logic of extract_nth_field(), using #defines.
    """
    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_NUM_CLIENTS_IDX)
    assert field == 'Summary: For 32 clients'

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_RUN_TYPE_IDX)
    assert field == 'Baseline - No logging'

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_NUM_THREADS_IDX)
    assert field == 'num_threads=1'

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_NUM_OPS_IDX)
    assert field == 'num_ops=32000000 (32 Million) ops'

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_ELAPSED_TIME_IDX)
    assert field == 'Elapsed time=105613818950 (~105.61 Billion) ns'

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_AVG_ELAPSED_TIME_IDX)
    assert field == 'Avg. Elapsed real time=3300 ns/msg'

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_SERVER_THROUGHPUT_IDX)
    assert field == 'Server throughput=302990 (~302.99 K) ops/sec'

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_CLIENT_THROUGHPUT_IDX)
    assert field == 'Client throughput=10035 (~10.03 K) ops/sec'

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE,
                                          perf_report.FLD_AVG_OPS_PER_THREAD_IDX)
    assert field == 'Avg. ops per thread=32000000 (32 Million) ops/thread'

    # Negative tests: Extraction should return None for out-of-band field indices
    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE, -1)
    assert field is None

    field = perf_report.extract_nth_field(SAMPLE_SUMM_LINE, 100)
    assert field is None


# #############################################################################
def test_extract_nclients_ops_fields():
    """
    Verify the extraction logic of extract_nclients_ops_fields()
    """
    (nclients_msg, nops_msg) = perf_report.extract_nclients_ops_fields(SAMPLE_SUMM_LINE)
    assert nclients_msg == 'For 32 clients'
    assert nops_msg == '32000000 (32 Million) msgs'

# #############################################################################
def test_gen_nthreads_list():
    """
    Verify the extraction logic of gen_nthreads_list(), which returns a list
    of server-thread parameters
    """
    # Create an input list of lines, with some duplicate rows for num_threads= field
    # and some unique rows.
    # pylint: disable=line-too-long
    flines = [ 'Summary: For 32 clients, Baseline - No logging, num_threads=1, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'
                , 'Summary: For 32 clients, Baseline - No logging, num_threads=1, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'

                , 'Summary: For 32 clients, Baseline - No logging, num_threads=2, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'
                , 'Summary: For 32 clients, Baseline - No logging, num_threads=2, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'

                , 'Summary: For 32 clients, Baseline - No logging, num_threads=8, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'
                ]
    # pylint: enable=line-too-long

    # Verify that the parsing routine returns this expected result.
    exp_list_of_nthreads = [ 'num_threads=1',  'num_threads=2', 'num_threads=8' ]

    list_of_nthreads = perf_report.gen_nthreads_list(flines)
    assert list_of_nthreads == exp_list_of_nthreads

# #############################################################################
def test_gen_runtypes_list():
    """
    Verify the extraction logic of gen_runtypes_list(), which returns a list
    of logging run-types
    """
    # Create an input list of the same lines, with some duplicate rows for run-type
    # fields and some unique rows.
    # pylint: disable=line-too-long
    flines = [
                'Summary: For 32 clients, spdlog-backtrace-logging, num_threads=8, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'


                , 'Summary: For 32 clients, L3-logging (no LOC), num_threads=2, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'

                , 'Summary: For 32 clients, L3-logging (no LOC), num_threads=2, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'

              ,  'Summary: For 32 clients, Baseline - No logging, num_threads=1, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'

                , 'Summary: For 32 clients, Baseline - No logging, num_threads=1, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread'

                ]
    # pylint: enable=line-too-long

    # Verify that the parsing routine returns this expected result, reordering
    # the run-types so that the Baseline row is the 1st one.
    exp_list_of_run_types = [  'Baseline - No logging'
                             , 'L3-logging (no LOC)'
                             , 'spdlog-backtrace-logging'
                            ]

    list_of_run_types = perf_report.gen_runtypes_list(flines)
    assert list_of_run_types == exp_list_of_run_types

    # 0th row is always expected to be the baseline (no-logging) run-type
    assert list_of_run_types[0].startswith('Baseline')

# #############################################################################
def test_strip_parens_etc():
    """
    Verify stripped string returned by strip_parens_etc()
    """

    item = '(~10.03 K) ops/sec'
    assert perf_report.strip_parens_etc(item) == '10.03 K ops/sec'

    item = '(10.03 K) ops/sec'
    assert perf_report.strip_parens_etc(item) == '10.03 K ops/sec'

    item = '10.03 K ops/sec'
    assert perf_report.strip_parens_etc(item) == '10.03 K ops/sec'

# #############################################################################
def test_extract_metrics():
    """
    Verify tuple returned by extract_metrics() on a summary output line.
    """
    # pylint: disable=line-too-long
    (svr_value, cli_value, svr_units_val, svr_units_str, cli_units_val, cli_units_str, thread_str) = perf_report.extract_metrics(SAMPLE_SUMM_LINE)

    print(f"'{svr_value}' '{cli_value}' '{svr_units_val}' '{svr_units_str} '{cli_units_val}' '{cli_units_str}' '{thread_str}'")

    # pylint: enable=line-too-long

    assert svr_value == 302990
    assert cli_value == 10035
    assert svr_units_val == 302.99
    assert svr_units_str == 'K ops/sec'
    assert cli_units_val == 10.03
    assert cli_units_str == 'K ops/sec'
    assert thread_str == '32 Million ops'

# #############################################################################
def test_line_matches():
    """
    Verify boolean returned by line_matches() on a summary output line.
    """
    assert perf_report.line_matches(SAMPLE_SUMM_LINE,
                                    'num_threads=1', 'Baseline - No logging') is True

    assert perf_report.line_matches(SAMPLE_SUMM_LINE,
                                    'num_threads=2', 'Baseline - No logging') is False

    assert perf_report.line_matches(SAMPLE_SUMM_LINE,
                                    'num_threads=1', 'L3-logging (no LOC)') is False
