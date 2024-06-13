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
from collections import OrderedDict
from collections import namedtuple
from prettytable import PrettyTable

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
    perf_file = parsed_args.file
    lctr = 0

    # Key: Server-thread-count 'NumThreads=1'; Value: metrics_by_run{}
    metrics_by_threads = OrderedDict()

    # Key: Server-thread-count 'NumThreads=1'; Value: namedtuple (svr_value, cli_value)
    baseline_tuple = namedtuple('baseline_tuple', 'svr_value cli_value')
    baseline_by_threads = OrderedDict()

    # Key: Run-type 'Baseline - No logging'; Value: [ <metrics> ]
    metrics_by_run = OrderedDict()
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
                metrics_by_threads[nthreads_field] = { run_type: [svr_value, cli_value, svr_str, cli_str, thread_str ] }
            else:
                by_nthreads_dict = metrics_by_threads[nthreads_field]
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

    print(f"{run_type}, {nthreads_field}")
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
                        , required=True
                        , help='Perf test-run output file name')

    parsed_args = parser.parse_args(args)
    if parsed_args is False:
        parser.print_help()

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

###############################################################################
# Start of the script: Execute only if run as a script
###############################################################################
if __name__ == "__main__":
    main()
