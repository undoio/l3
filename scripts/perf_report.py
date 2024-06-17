#!/usr/bin/env python3
"""
Python script to parse file containing one-line summary of metrics from
client-server performance u-benchmarking tests.
Generate summary comparison report.

Date 2024-05-25
Copyright (c) 2024
"""
import sys
import argparse
import statistics
from collections import OrderedDict
from collections import namedtuple
from prettytable import PrettyTable

# ##########################################################
# Layout of a summary row from the output file:
# ##########################################################
# pylint: disable-next=line-too-long
# Summary: For 32 clients, Baseline - No logging, num_threads=1, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread

# Define indexes for each field in above row-format
FLD_NUM_CLIENTS_IDX         = 0
FLD_RUN_TYPE_IDX            = 1
FLD_NUM_THREADS_IDX         = 2
FLD_NUM_OPS_IDX             = 3
FLD_ELAPSED_TIME_IDX        = 4
FLD_AVG_ELAPSED_TIME_IDX    = 5
FLD_SERVER_THROUGHPUT_IDX   = 6
FLD_CLIENT_THROUGHPUT_IDX   = 7
FLD_AVG_OPS_PER_THREAD_IDX  = 8

# -----------------------------------------------------------------------------
# Define a named tuple class for metrics extracted out from a summary line.
# pylint: disable-next=line-too-long
MetricsTuple = namedtuple('MetricsTuple', 'svr_value cli_value svr_units_val svr_units_str cli_units_val cli_units_str thread_str')

# Define a named tuple class for aggregated metrics for a given metric value.
# niters is the # of raw-data-rows that went into computing the aggregates.
AggMetrics = namedtuple('AggMetrics', 'niters, min_val, avg_val, med_val, max_val')

# Define a named tuple class for the set-of-aggregated metrics from a set of
# summary line-metrics that are squashed into a set of AggMetrics named tuples.
# pylint: disable-next=line-too-long
AggMetricsTuple = namedtuple('AggMetrics', 'svr_value_agg cli_value_agg svr_units_val_agg svr_units_str cli_units_val_agg cli_units_str thread_str')

###############################################################################
# main() driver
###############################################################################
def main():
    """
    Shell to call do_main() with command-line arguments.
    """
    do_main(sys.argv[1:])

###############################################################################
# pylint: disable-msg=too-many-locals
def do_main(args:list):
    """
    Main method that drives parsing the input file to generate the report.
    Each line is of the format:
     - Multiple comma-separated fields
     - Each field is '='-separated, like so; "<metric>=<value> (<text>)"

# pylint: disable=line-too-long

Baseline - No logging, NumClients=5, NumOps=5000000 (5 Million), Server throughput=91795 (~91.79 K) ops/sec, Client throughput=20938 (~20.93 K) ops/sec, elapsed_ns=54469084570 (~54.46 Billion) ns

L3-logging (no LOC), NumClients=5, NumOps=5000000 (5 Million), Server throughput=98602 (~98.60 K) ops/sec, Client throughput=24414 (~24.41 K) ops/sec, elapsed_ns=50708895912 (~50.70 Billion) ns

# pylint: enable=line-too-long
    Parse each comma-separated fields and extract values.
    """
    parsed_args = perf_parse_args(args)
    perf_summ_file = parsed_args.summ_file
    if perf_summ_file is not None:
        gen_perf_report_from_summary_file(perf_summ_file)
        sys.exit(0)

    perf_file = parsed_args.file
    lctr = 0

    # Key: Server-thread-count 'NumThreads=1'; Value: metrics_by_run{}
    metrics_by_threads = OrderedDict()

    # Key: Server-thread-count 'NumThreads=1'; Value: namedtuple (svr_value, cli_value)
    baseline_tuple = namedtuple('baseline_tuple', 'svr_value cli_value')
    baseline_by_threads = OrderedDict()

    # Key: Run-type 'Baseline - No logging'; Value: [ <metrics> ]
    heading_row = []

    # Setup some defaults to avoid pylint errors.
    nclients_field = 'NumClients=5'
    nops_field = 'NumOps=5000'

    # ------------------------------------------------------------------------
    # We expect a consistent format of one-line summary messages arranged by:
    #
    #   - Run-Type-1: < Set of metrics for NumThreads=1 >
    #   - Run-Type-1: < Set of metrics for NumThreads=2 >
    #   - Run-Type-1: < Set of metrics for NumThreads=4 >
    #   - Run-Type-1: < Set of metrics for NumThreads=8 >
    #
    #   - Run-Type-2: < Set of metrics for NumThreads=1 >
    #   - Run-Type-2: < Set of metrics for NumThreads=2 >
    #   - Run-Type-2: < Set of metrics for NumThreads=4 >
    #   - Run-Type-2: < Set of metrics for NumThreads=8 >
    # and so on ...
    # ------------------------------------------------------------------------
    with open(perf_file, 'rt', encoding="utf-8") as file:
        for line in file:
            if lctr == 0:

                # pylint: disable-next=line-too-long
                (nclients_field, nops_field, nthreads_field, svr_metric, cli_metric, thread_metric) = parse_perf_line_names(line)
                heading_row = [ 'Run-Type', svr_metric, cli_metric, thread_metric ]

            (run_type, nthreads_field, svr_value, svr_str, cli_value, cli_str, thread_str) = \
                parse_perf_line_values(line)

            # Create hash-entry for baseline server/client metrics for this thread-count
            if run_type.startswith('Baseline'):
                # svr_value - baseline server-throughput
                # cli_value - baseline client-throughput
                baseline_by_threads[nthreads_field] = baseline_tuple(svr_value, cli_value)

            # DEBUG: print(f"{run_type}, {thread_str}")

            # ------------------------------------------------------------------
            # We want to distribute the metrics for each run-type across the
            # map of metrics arranged by #-of-server-threads.
            #
            # - If this thread-count is a new one, add the dictionary-entry
            #   { run_type: [ <metrics> ] } to the metrics tracked per thread.
            # - Otherwise, add the metrics for this run-type, to the map of
            #   metrics seen before for other run-types, for this server
            #   thread-count.
            #
            # This way, we will be rearranging the one-line metrics rows,
            # read from the summary file, into a dictionary of dictionaries,
            # arranged by thread-count.
            # ------------------------------------------------------------------
            by_nthreads_dict = OrderedDict()
            if nthreads_field not in metrics_by_threads:
                # pylint: disable-next=line-too-long
                metrics_by_threads[nthreads_field] = { run_type: [svr_value, cli_value, svr_str, cli_str, thread_str ] }
            else:
                by_nthreads_dict = metrics_by_threads[nthreads_field]
                # pylint: disable-next=line-too-long
                by_nthreads_dict.update( {run_type: [svr_value, cli_value, svr_str, cli_str, thread_str ] } )
                metrics_by_threads[nthreads_field] = by_nthreads_dict

            # DEBUG: pr_metrics_by_thread(metrics_by_threads)
            lctr += 1

    # DEBUG: pr_metrics_by_thread(metrics_by_threads)

    # pylint: disable-next=line-too-long
    gen_perf_report_by_threads(heading_row, metrics_by_threads, baseline_by_threads, nclients_field, nops_field)

# #############################################################################
def parse_perf_line_names(line:str) -> tuple:
    """
    Parse a one-line summary line and return a tuple of common, useful, field-names.

    Returns: A tuple; see below.
        nclients_field  : str   : 'NumClients=5'
        nops_field      : str   : 'NumOps=5000000 (5 Million)'
        nthreads_field  : str   : 'NumThreads=2'
        svr_metric      : str   : 'Server throughput'
        cli_metric      : str   : 'Client throughput'
        nops_per_thread : str   : 'NumOps/thread=25000 (25 K)'
    """

    # pylint: disable-next=unused-variable,line-too-long
    (run_type_unused, nclients_field, nops_field, nthreads_field, svr_field, cli_field, time_unused, nops_per_thread) = line.split(',')

    nclients_field = nclients_field.lstrip().rstrip()
    nops_field = nops_field.lstrip().rstrip()
    nthreads_field = nthreads_field.lstrip().rstrip()

    svr_field = svr_field.lstrip().rstrip()

    # pylint: disable-next=unused-variable
    (svr_metric, unused) = svr_field.split('=')

    cli_field = cli_field.lstrip().rstrip()
    (cli_metric, unused) = cli_field.split('=')

    nops_per_thread = nops_per_thread.lstrip().rstrip()
    (thread_metric, unused) = nops_per_thread.split('=')

    return (nclients_field, nops_field, nthreads_field, svr_metric, cli_metric, thread_metric)

# #############################################################################
def parse_perf_line_values(line:str) -> tuple:
    """
    Parse a one-line summary line and return a tuple of useful values.
    Returns: A tuple; see below.
        run_type    : str   : 'Baseline - No logging'
        svr_value   : int   : 91795
        svr_str     : str   : '(~91.79 K) ops/sec'
        cli_value   : int   : 20938
        cli_str     : str   : '(~20.93 K) ops/sec'
        thread_str  : str   : '25 K'
    """
    # pylint: disable-next=unused-variable,line-too-long
    (run_type, ct_unused, nops_unused, nthreads_field, svr_field, cli_field, time_field_unused, nops_per_thread) = line.split(',')

    run_type = run_type.lstrip().rstrip()
    nthreads_field = nthreads_field.lstrip().rstrip()

    (svr_value_str, svr_str) = split_field_value_str(svr_field)
    (cli_value_str, cli_str) = split_field_value_str(cli_field)

    # pylint: disable-next=unused-variable
    (unused, thread_str) = split_field_value_str(nops_per_thread)

    svr_value = int(svr_value_str)
    cli_value = int(cli_value_str)

    # DEBUG: print(f"{run_type}, {nthreads_field}")
    return (run_type, nthreads_field, svr_value, svr_str, cli_value, cli_str, thread_str)

# #############################################################################
def split_field_value_str(metric_field:str) -> (int, str):
    """
    Specialized string-'split' method.
    Common case:
      Input:  'Server throughput=53856 (~53.85 K) ops/sec'
      Returns:    '~53.85 K ops/sec'

    Lapsed case:
      Input:  'Server throughput=989 () ops/sec'
      Returns:    '989 ops/sec'
    """

    metric_field = metric_field.lstrip().rstrip()
    # pylint: disable-next=unused-variable
    (svr_metric_name_unused, rest) = metric_field.split('=')

    svr_value_str = rest.split(' ')[0]
    svr_value = int(svr_value_str)
    svr_str = None
    if len(rest.split(' ')) > 1:
        # Will resolve to one of: '~53.85 K) ops/sec' or ') ops/sec'
        svr_str = rest.split('(')[1]
        svr_str = svr_str.lstrip().rstrip()

        # Lapsed case: If unit-specifier value in '()' is empty, use the
        # metric's value as the output. Here return '989 ops/sec'
        if svr_str.startswith(')'):
            svr_str = svr_value_str + svr_str.replace(')', '')
        else:
            svr_str = svr_str.replace(')', '')

    return (svr_value, svr_str)

# #############################################################################
def gen_perf_report_by_threads(heading:list, metrics_by_threads:dict, baseline_by_threads:dict,
                               nclients_field: str, nops_field: str):
    """
    Given a dictionary of metrics_by_threads, ordered by # of server-threads,
    generate the comparative performance reports for metrics gathered for diff
    logging types for this server-thread count.

    Parameters:
      metrics_by_threads:   Dictionary ordered by server-thread count.
                            Value is another dictionary, 'metrics'.
                            { 'run_type' -> [ < metrics > ] }
    """
    # Key nthreads_field will be: 'NumThreads=1', 'NumThreads=2' etc.
    for nthreads_field in metrics_by_threads.keys():

        base_svr_value = baseline_by_threads[nthreads_field].svr_value
        base_cli_value = baseline_by_threads[nthreads_field].cli_value
        if base_svr_value is None or base_cli_value is None:
            print("Error: Throughputs metrics for baseline run are not known.")
            sys.exit(1)

        # pylint: disable-next=line-too-long
        print(f"    **** Performance comparison for {nclients_field}, {nops_field}, {nthreads_field} ****")
        gen_perf_report(heading, metrics_by_threads[nthreads_field], base_svr_value, base_cli_value)
        print("\n")

# #############################################################################
def gen_perf_report(heading:list, metrics:dict, base_svr_value:int, base_cli_value:int):
    """
    Given a dictionary of metrics, ordered by run-type 'key', generate the
    comparative performance reports.
    Ref: https://www.geeksforgeeks.org/how-to-make-a-table-in-python/
         https://ptable.readthedocs.io/en/latest/tutorial.html
    """
    srv_head = 'Srv:Drop'
    cli_head = 'Cli:Drop'
    perf_table = PrettyTable([heading[0], heading[1], srv_head, heading[2], cli_head, heading[3]])

    # Key run_type will be: 'Baseline - No logging' etc.
    for run_type in metrics.keys():
        svr_value  = metrics[run_type][0]
        cli_value  = metrics[run_type][1]
        svr_str    = metrics[run_type][2]
        cli_str    = metrics[run_type][3]
        thread_str = metrics[run_type][4]
        perf_table.add_row([  run_type
                            , svr_str, compute_pct_drop(base_svr_value, svr_value)
                            , cli_str, compute_pct_drop(base_cli_value, cli_value)
                            , thread_str])

    perf_table.align[heading[0]] = "l"
    perf_table.custom_format[srv_head] = lambda f, v: f"{ v:.2f} %"
    perf_table.custom_format[cli_head] = lambda f, v: f"{ v:.2f} %"

    print(perf_table)

# #############################################################################
# pylint: disable=line-too-long
def gen_perf_report_from_summary_file(perf_summ_file:str):
    """
    Driver method to parse `Summary:` lines from perf_summ_file to extract out
    the metrics gathered at run-time. Generates a summary performance
    comparison report.

    The input file is expected to have multiple lines of this format:

Summary: For 32 clients, Baseline - No logging, num_threads=1, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread

Summary: For 32 clients, Baseline - No logging, num_threads=2, num_ops=32000000 (32 Million) ops, Elapsed time=69448583392 (~69.44 Billion) ns, Avg. Elapsed real time=2170 ns/msg, Server throughput=460772 (~460.77 K) ops/sec, Client throughput=15524 (~15.52 K) ops/sec, Avg. ops per thread=16000000 (16 Million) ops/thread

    The layout of this file is as follows:
    For a given # of clients: e.g., 'For 32 clients':

     - One summary-line:
        - for each logging-type
        - for one server-thread-count
        - for 1 or more iterations of server execution

     - For one run-parameter: (logging-type, num-server-threads), we could have
       1 or more lines, for each iteration of the test. (Iteration lines are not
       tagged.)

     - After n-iterations, we could move to the next num-server-threads count.

     - If the experiment was run with just one thread-count; i.e., env-var
       L3_PERF_SERVER_NUM_THREADS=4, we will have just one set of these rows
       for a single thread-count.

    We have to rearrange the metrics rows so that the performance comparison
    report for each logging-type as compared to baseline performance is
    generated for different server-thread counts.

    For runs with multiple iterations, this method computes the median metric
    value and re-casts the metrics to drive-off of that. We massage the computed
    median metric(s) to invoke existing report-generation code.

    """
    # pylint: enable=line-too-long

    # Create an in-memory list of lines, which we'll scan multiple times.
    flines = read_summary_lines(perf_summ_file)

    if len(flines) == 0:
        return

    (num_clients_msg, num_ops_msg) = extract_nclients_ops_fields(flines[0])

    (svr_metric_name, cli_metric_name, thread_metric_name) = extract_metric_names(flines[0])
    heading_row = [ 'Run-Type', svr_metric_name, cli_metric_name, thread_metric_name ]

    # Returns 'Baseline' run-type as 0'th row.
    list_of_nthreads = gen_nthreads_list(flines)
    # DEBUG: print(list_of_nthreads)

    list_of_runtypes = gen_runtypes_list(flines)
    # DEBUG: print(list_of_runtypes)

    base_svr_value = 0
    base_cli_value = 0

    # Run through diff server-thread parameters, and generate report
    # We make multiple passes extracing metrics for each combination
    # of #-of-server-threads and run-types.
    for threads in list_of_nthreads:

        metrics_by_run_type = OrderedDict()

        for run_type in list_of_runtypes:

            list_of_metrics = extract_metrics_for(flines, threads, run_type)

            # No. of iterations done for each client/server combo run-type
            num_iters = len(list_of_metrics)

            agg_metrics = compute_aggregates(list_of_metrics)

            run_type_metrics_list = extract_med_from_agg_metrics(agg_metrics)
            # DEBUG: print(run_type_metrics_list)

            if run_type.startswith('Baseline'):
                base_svr_value = run_type_metrics_list[0]
                base_cli_value = run_type_metrics_list[1]

            metrics_by_run_type[run_type] = run_type_metrics_list

        # sign = 'positive' if x > 0 else 'non-positive'
        one_iters_msg = "(1 iteration)" if num_iters == 1 else ""

        # pylint: disable-next=line-too-long
        print(f"\n    **** Performance comparison for {num_clients_msg}, {num_ops_msg}, {threads} {one_iters_msg} ****")
        if num_iters > 1:
            print(f"        **** (Using median value of metric across {num_iters} iterations) ****")
        gen_perf_report(heading_row, metrics_by_run_type, base_svr_value, base_cli_value)


# #############################################################################
def read_summary_lines(perf_summ_file:str) -> list:
    """
    Read the summary lines, filtering out comment lines.

    Returns: A list of lines.
    """
    flines = []
    with open(perf_summ_file, 'rt', encoding="utf-8") as file:
        flines = [line for line in file.readlines() if not line.startswith('#')]
    return flines

# #############################################################################
def extract_nclients_ops_fields(line:str) -> tuple:
    """
    Parse an input line to extract out the number of clients and number of
    msgs (ops) fields and return a tuple to the caller
    Returns: Tuple: ('For 32 clients', 'num_ops=32000000 (32 Million) msgs')
    """
    nclients_msg = extract_nth_field(line, FLD_NUM_CLIENTS_IDX)
    nops_msg = extract_nth_field(line, FLD_NUM_OPS_IDX)

    # Further massage fields to get output data as described above.
    nclients_msg = nclients_msg.split(':')[1]
    nclients_msg = nclients_msg.lstrip().rstrip()

    nops_msg = nops_msg.split('=')[1]
    nops_msg = nops_msg.split(')')[0] + ') msgs'
    nops_msg = nops_msg.lstrip().rstrip()
    return (nclients_msg, nops_msg)

# #############################################################################
def extract_metric_names(line:str) -> tuple:
    """
    Parse the input line and extract the names of server / client metric.
    Synthesize the server-thread metric's name.
    Returns: (svr_metric_name, cli_metric_name, thread_metric_name)
    """
    svr_throughput_str = extract_nth_field(line, FLD_SERVER_THROUGHPUT_IDX)
    svr_throughput_str = svr_throughput_str.split('=')[0]

    cli_throughput_str = extract_nth_field(line, FLD_CLIENT_THROUGHPUT_IDX)
    cli_throughput_str = cli_throughput_str.split('=')[0]

    thread_throughput_str = 'NumOps/thread'

    return (svr_throughput_str, cli_throughput_str, thread_throughput_str)


# #############################################################################
def extract_nth_field(line:str, field_num:int) -> tuple:
    """
    Extract out the n'th field from a comma-separated set of fields from line.

    Returns:
        - The field, if found, with leading / trailing spaces trimmed off.
        - None otherwise
    """
    fields = line.split(",")
    if field_num < 0 or len(fields) < field_num:
        return None

    field_str = fields[field_num]
    field_str = field_str.lstrip().rstrip()
    return field_str

# #############################################################################
def gen_nthreads_list(flines:list) -> list:
    """
    Process the list of input lines and identify the list of server-threads for
    which the tests were run.
    Returns: List of 'num_threads=<n>' items.
    """
    # Generate list of threads using list comprehension.
    seen_threads = set()
    list_of_nthreads = [ nthreads_term
                         for line in flines
                         if (nthreads_term := extract_nth_field(line, FLD_NUM_THREADS_IDX))
                            not in seen_threads and not seen_threads.add(nthreads_term)
                       ]

    return list_of_nthreads

# #############################################################################
def gen_runtypes_list(flines:list) -> list:
    """
    Process the list of input lines and identify the list of logging run-types
    for which the tests were run.
    Returns: List of run-types, keeping the Baseline list at [0]'th slot.
    """
    # Generate list of run-types using list comprehension.
    seen_run_types = set()
    list_of_run_types = [ run_type
                         for line in flines
                         if (run_type := extract_nth_field(line, FLD_RUN_TYPE_IDX))
                            not in seen_run_types and not seen_run_types.add(run_type)
                        ]

    # Re-arrange so that baseline row is the 1st item in the list
    if list_of_run_types[0].startswith('Baseline') is False:
        baseline_off = -1
        rctr = 0
        for runtype in list_of_run_types:
            if runtype.startswith('Baseline'):
                baseline_off = rctr
                break

            rctr += 1

        # Flip rows, if baseline record is found
        if baseline_off != -1:
            baseline_row = list_of_run_types[baseline_off]
            list_of_run_types[baseline_off] = list_of_run_types[0]
            list_of_run_types[0] = baseline_row

    return list_of_run_types


# #############################################################################
def extract_metrics_for(flines:list, nthreads:str, run_type:str) -> list:
    """
    Given a list of summary lines, #-of-threads parameter and run-type,
    extract out the relevants metrics for matching summary-lines.

    Returns: List of MetricsTuple() named tuples for metrics.
    """
    metrics = []
    for line in flines:
        if line_matches(line, nthreads, run_type) is False:
            continue

        (svr_value, cli_value, svr_units_val, svr_units_str, cli_units_val, cli_units_str, thread_str) = extract_metrics(line)
        metrics.append(MetricsTuple(svr_value, cli_value, svr_units_val, svr_units_str, cli_units_val, cli_units_str, thread_str))

    # print(f"\nList of metrics tuple(s) for {nthreads}, {run_type}")
    # print_list_of_metrics_tuples(metrics)
    return metrics

# #############################################################################
def compute_aggregates(list_of_metrics:list) -> AggMetricsTuple:
    """
    Given a list of MetricsTuple() tuples, compute the required aggregates for
    each interesting metric.

    Returns: Tuple of type AggMetrics()
    """
    # DEBUG: print("\nList of metrics tuple(s)")
    # DEBUG: print_list_of_metrics_tuples(list_of_metrics)

    num_list = [ mtuple.svr_value for mtuple in list_of_metrics ]
    # DEBUG: print(sorted(num_list))

    svr_value_agg = aggregate_num_list(num_list)
    # DEBUG: print(f"{svr_value_agg}\n")

    num_list = [ mtuple.cli_value for mtuple in list_of_metrics ]
    # DEBUG: print(sorted(num_list))

    cli_value_agg = aggregate_num_list(num_list)
    # DEBUG: print(f"{cli_value_agg}\n")

    num_list = [ mtuple.svr_units_val for mtuple in list_of_metrics ]
    # DEBUG: print(sorted(num_list))

    svr_units_val_agg = aggregate_num_list(num_list)
    # DEBUG: print(f"{svr_units_val_agg}\n")

    num_list = [ mtuple.cli_units_val for mtuple in list_of_metrics ]
    # DEBUG: print(sorted(num_list))

    cli_units_val_agg = aggregate_num_list(num_list)
    # DEBUG: print(f"{cli_units_val_agg}\n")

    # All server/client units-value have the same units. Grab the 1st one.
    for mtuple in list_of_metrics:
        svr_units_str = mtuple.svr_units_str
        cli_units_str = mtuple.cli_units_str

    # We expect that the distribution of #-ops processed / thread to be uniform.
    # So, do a loose check that we got a unique value. And warn the user if
    # that's not the case.
    ops_per_thread_set = set()
    ops_per_thread_list = [ mtuple.thread_str
                            for mtuple in list_of_metrics
                            if mtuple.thread_str not in ops_per_thread_set
                            and not ops_per_thread_set.add(mtuple.thread_str)
                          ]

    nunique_ops_per_thread = len(ops_per_thread_list)
    if nunique_ops_per_thread > 1:
        print("Warning! Found {nunique_ops_per_thread} ops/thread metric.")

    thread_str = ops_per_thread_list[0]
    # DEBUG: print(thread_str)

    return AggMetricsTuple(svr_value_agg, cli_value_agg, svr_units_val_agg, svr_units_str, cli_units_val_agg, cli_units_str, thread_str)

# #############################################################################
def aggregate_num_list(list_of_nums:list) -> AggMetrics:
    """
    Given a list of numbers (ints/floats), compute their aggregates.
    Returns: AggMetrics() tuple
    """
    nitems = len(list_of_nums)
    if nitems == 0:
        return None

    min_num = sys.maxsize
    max_num = 0
    avg_num = 0
    med_num = 0
    sum_num = 0
    for num_val in list_of_nums:
        min_num = min(min_num, num_val)
        max_num = max(max_num, num_val)
        sum_num += num_val

    med_num = statistics.median(list_of_nums)
    avg_num = round(((sum_num * 1.0) / nitems), 2)

    return AggMetrics(nitems, min_num, avg_num, med_num, max_num)

# #############################################################################
def extract_metrics(line:str) -> tuple:
    """
    Given a summary line, extract key-metrics fields that will be needed to
    compute aggregates and also for reporting.

    Example: For a line like this:

# pylint: disable-next=line-too-long
# Summary: For 32 clients, Baseline - No logging, num_threads=1, num_ops=32000000 (32 Million) ops, Elapsed time=105613818950 (~105.61 Billion) ns, Avg. Elapsed real time=3300 ns/msg, Server throughput=302990 (~302.99 K) ops/sec, Client throughput=10035 (~10.03 K) ops/sec, Avg. ops per thread=32000000 (32 Million) ops/thread

    Returns: Tuple:
        - svr_value     : int   : 302990
        - cli_value     : int   : 10035
        - svr_units_val : float : 302.99
        - svr_units_str : str   : 'K ops/sec'
        - cli_units_val : float : 10.03
        - cli_units_str : str   : 'K ops/sec'
        - thread_str    : str   : '32 Million ops'
    """
    svr_throughput_str = extract_nth_field(line, FLD_SERVER_THROUGHPUT_IDX)
    svr_throughput_str = svr_throughput_str.split('=')[1]

    (svr_value, svr_units_str) = svr_throughput_str.split('(')
    svr_value = int(svr_value.lstrip().rstrip())
    svr_units_str = strip_parens_etc(svr_units_str)
    (svr_units_val, svr_units_str) = svr_units_str.split(sep=' ', maxsplit=1)
    svr_units_val = float(svr_units_val)

    cli_throughput_str = extract_nth_field(line, FLD_CLIENT_THROUGHPUT_IDX)
    cli_throughput_str = cli_throughput_str.split('=')[1]

    (cli_value, cli_units_str) = cli_throughput_str.split('(')
    cli_value = int(cli_value.lstrip().rstrip())
    cli_units_str = strip_parens_etc(cli_units_str)
    (cli_units_val, cli_units_str) = cli_units_str.split(sep=' ', maxsplit=1)
    cli_units_val = float(cli_units_val)

    avg_ops_per_thread_str = extract_nth_field(line, FLD_AVG_OPS_PER_THREAD_IDX)
    thread_str = avg_ops_per_thread_str.split('=')[1]

    # pylint: disable-next=unused-variable
    (thread_val_unused, thread_str) = thread_str.split(sep=' ', maxsplit=1)
    thread_str = strip_parens_etc(thread_str)
    thread_str = thread_str.split('/')[0]

    return (svr_value, cli_value, svr_units_val, svr_units_str, cli_units_val, cli_units_str, thread_str)

# #############################################################################
def extract_med_from_agg_metrics(agg_metrics:AggMetricsTuple) -> list:
    """
    Given a named-tuple, AggMetricsTuple, of aggregated metrics, extract the
    key metrics fields that are needed for report generation. We use the
    median value of the metric to generate performance gain/drop %age.

    Returns: List of metric values in a form as needed by downstream code.
    """
    svr_value = agg_metrics.svr_value_agg.med_val
    cli_value = agg_metrics.cli_value_agg.med_val
    svr_str   = str(agg_metrics.svr_units_val_agg.med_val) + ' ' + agg_metrics.svr_units_str
    cli_str   = str(agg_metrics.cli_units_val_agg.med_val) + ' ' + agg_metrics.cli_units_str
    thread_str = agg_metrics.thread_str

    return (svr_value, cli_value, svr_str, cli_str, thread_str)

# #############################################################################
def strip_parens_etc(field_str:str) -> str:
    """
    Given an input like: '(~10.03 K) ops/sec', strips away stuff and
    Returns: '10.03 K ops/sec'
    """
    field_str = field_str.replace('(', '')
    field_str = field_str.replace(')', '')
    field_str = field_str.replace('~', '')
    return field_str

# #############################################################################
def line_matches(line:str, nthreads:str, run_type:str) -> bool:
    """
    See if input line matches the desired #-of-threads and run-type.
    """
    nthreads_field = extract_nth_field(line, FLD_NUM_THREADS_IDX)
    run_type_field = extract_nth_field(line, FLD_RUN_TYPE_IDX)
    return (nthreads == nthreads_field) and (run_type == run_type_field)

# #############################################################################
def compute_pct_drop(baseval:int, metric:int) -> float:
    """
    Compute the %age drop of new metric v/s baseline value 'baseval'.
    Returns: Percentage drop as an int
    """
    return round((((metric - baseval) / baseval) * 100.0), 2)

# #############################################################################
def perf_parse_args(args:list):
    """
    Parse command-line arguments. Return parsed-arguments object
    """
    # ---------------------------------------------------------------
    # Start of argument parser, with inline examples text
    # Create 'parser' as object of type ArgumentParser
    # ---------------------------------------------------------------
    parser  = argparse.ArgumentParser(description='Generate summary performance'
                                                 + ' report',
                                      formatter_class=argparse.RawDescriptionHelpFormatter,
                                      epilog=r'''Examples:

- Basic usage:
    ''' + sys.argv[0]
        + ''' --file <run-all-client-server-perf-tests-file>
''')

    # ======================================================================
    # Define required arguments supported by this script
    parser.add_argument('--file', dest='file'
                        , metavar='<perf-outfile-name>'
                        , required=False
                        , default=None
                        , help='Perf test-run output file name')

    parser.add_argument('--summary-file', dest='summ_file'
                        , metavar='<stdout-grep-Summary-outfile-name>'
                        , required=False
                        , default=None
                        , help='Perf output file name with "Summary:" lines')

    parsed_args = parser.parse_args(args)
    if parsed_args is False:
        parser.print_help()

    # Either one of the file-specifiers should be provided
    if parsed_args.file is None and parsed_args.summ_file is None:
        # pylint: disable-next=line-too-long
        print(f"{sys.argv[0]} Error: Either one of --file or --summary-file arguments must be provided.")
        sys.exit(1)

    if parsed_args.file is not None and parsed_args.summ_file is not None:
        # pylint: disable-next=line-too-long
        print(f"{sys.argv[0]} Error: Only one of --file or --summary-file arguments must be provided.")
        sys.exit(1)

    return parsed_args

# #############################################################################
def pr_metrics_by_thread(tdict:dict):
    """
    Print an ordered dictionary, where value is another ordered dictionary.
    metrics_by_threads{'NumThreads=1'} =
        metrics_by_run{'Baseline - No logging'} = [ <values> ]
    """
    for key in tdict.keys():
        print(f"{key=}, value = [")
        pr_metrics(tdict[key], "  ")
        print("]")

# #############################################################################
def pr_metrics(tdict:dict, indent:str = ""):
    """
    Print an ordered dictionary, where value is a tuple.
    """
    for key in tdict.keys():
        print(f"{indent}{key=}, value = {tdict[key]}")

# #############################################################################
def print_list_of_metrics_tuples(metrics:list, indent:str = "  "):
    """ Print contents of a list of MetricsTuple()s"""
    print("[")
    for mtuple in metrics:
        # This will print using Python's native print method.
        print(f"{indent}{mtuple}")

        # Print by unpacking each field
        # pylint: disable-next=line-too-long
        # print(f"{indent}svr_value={mtuple.svr_value}, cli_value={mtuple.cli_value}, svr_units_val={mtuple.svr_units_val}, cli_units_val={mtuple.cli_units_val}, units_str={mtuple.cli_units_str}, thread_str={mtuple.thread_str}")
    print("]")

###############################################################################
# Start of the script: Execute only if run as a script
###############################################################################
if __name__ == "__main__":
    main()
