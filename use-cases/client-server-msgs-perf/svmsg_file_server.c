/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2024.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* ****************************************************************************
 * svmsg_file_server.c
 * ****************************************************************************

   The server and client communicate using System V message queues.

   This version of the application has been extensively modified based
   on an initial version downloaded from:
   https://man7.org/tlpi/code/online/all_files_by_chapter.html

Usage: ./svmsg_file_server --help
       ./svmsg_file_server [  --clock-default
                            | --clock-monotonic
                            | --clock-realtime
                            | --clock-process-cputime-id
                            | --clock-thread-cputime-id ]

   NOTE: On Linux, this program supports cmdline flags --clock-<something> to select
         a different clock to "measure" time taken to implement the message.

         The clock is hard-coded on Mac/OSX to CLOCK_THREAD_CPUTIME_ID:

          a) This is the only clock with 1ns clock resolution on Mac.
          b) Mac/OSX system call seems to not work well with `clock_id` int and
             needs a mnemonic. So, parametrizing the call using `clock_id`
             selected by a cmdline flag did not seem practical.

   The application is a very simple process:
    client -- counter --> server (incr/decr) -> respond new counter to client

   A file server that uses System V message queues to handle client requests
   (see svmsg_file_client.c).

   The client sends an initial request containing:
        - An initial counter
        - A message type indicating to increment / decrement the counter or
          to exit. (And possibly a few more defined in req_resp_type_t{}.
        - The identifier of the message queue to be used to send the response
          back to the child.

   The server does basic book-keeping:
        - For incr / decr request, do the appropriate math. Return counter
        - For exit request, manage # of active client conns etc.

   When all clients exit the system, the server will cleanup message queues
   and exit normally.

   This application makes use of multiple message queues. The server maintains
   a queue (with a well-known key) dedicated to incoming client requests. Each
   client creates its own private queue, which is used to pass response
   messages from the server back to the client.

   This program operates as a serialized server, processing all client requests
   serially.
 * ****************************************************************************
*/
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <assert.h>
#include <getopt.h>     // For getopt_long()
#include "svmsg_file.h"
#include "l3.h"
#include "size_str.h"

/**
 * Global data structures to track client-specific information.
 *
 * NOTE: This program can be configured to use different clocks to measure
 * time, using the --clock<arg> argument. The cumu_time_ns tracks the
 * cumulative time elapsed to implement the msg, including the time for
 * any logging that might have been done in a build/experiment.
 */
typedef struct client_info {
    int             clientId;
    int             client_idx;         // Into ActiveClients[] array, below
    int64_t         client_ctr;         // Current value of client's counter
    uint64_t        cumu_time_ns;       // Cumulative elapsed-time for operation,
                                        // including logging overheads.
    uint64_t        num_ops;            // # of msg-op requests received
    uint64_t        throughput;         // Client-side avg throughput nops/sec
    req_resp_type_t last_mtype;         // Last request's->mtype
} Client_info;

Client_info ActiveClients[MAX_CLIENTS];

/**
 * We have a simplistic model to track clients connecting and exiting.
 *
 * - NumActiveClientsHWM: High-Water mark of # of active clients.
 *   When client attaches to server, use NumActiveClientsHWM as index
 *   into above array for this client's info.
 *   Bump up both NumActiveClients and NumActiveClientsHWM.
 *
 * - NumActiveClients: Current # of active clients connected.
 *   When a client exits, decrement NumActiveClients, but do not
 *   decrement NumActiveClientsHWM. This way next client who enters the
 *   system will use next Client_info[] slot, without trying to reuse
 *   the slot(s) freed by clients who exited previously.
 */
static unsigned int   NumActiveClientsHWM = 0;
static unsigned int   NumActiveClients    = 0;

/**
 * Simple argument parsing structure.
 */
struct option Long_options[] = {
    // Options that need no arguments
      { "clock-default"             , no_argument         , NULL, 'd'}
    , { "help"                      , no_argument         , NULL, 'h'}
    , { "clock-monotonic"           , no_argument         , NULL, 'm'}
    , { "clock-process-cputime-id"  , no_argument         , NULL, 'p'}
    , { "clock-realtime"            , no_argument         , NULL, 'r'}
    , { "clock-thread-cputime-id"   , no_argument         , NULL, 't'}

    // Options that need arguments
    , { "perf-outfile"              , required_argument   , NULL, 'o'}
    , { NULL, 0, NULL, 0}           // End of options
};

const char * Options_str = "o:dhmprt";

// Useful macros
#define ARRAY_LEN(arr)  (sizeof(arr) / sizeof(*arr))

// Function Prototypes

int parse_arguments(const int argc, char *argv[], int *clock_id, char **outfile);
void print_usage(const char *program, struct option options[]);
void printSummaryStats(const char *outfile, const char *run_descr,
                       Client_info *clients, unsigned int num_clients,
                       int clock_id, uint64_t elapsed_ns);

void svr_clock_calibrate(void);
unsigned svr_clock_overhead(clockid_t clock_id);
const char *clock_name(int clock_id);
const char *time_metric_name(int clock_id);

static void             /* SIGCHLD handler */
grimReaper(int sig)
{
    int savedErrno;

    savedErrno = errno;                 /* waitpid() might change 'errno' */
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        continue;
    }
    errno = savedErrno;
}

/**
 * ****************************************************************************
 * main() - Server program.
 * ****************************************************************************
 */
int
main(int argc, char *argv[])
{
    ssize_t msgLen;
    int     serverId;
    struct sigaction sa;

#if __APPLE__
    printf("%s is currently not supported on Mac/OSX\n", argv[0]);
    exit(EXIT_SUCCESS);
#endif

    // On MacOSX, only the CLOCK_THREAD_CPUTIME_ID clock has a resolution of 1ns.
    // On Linux, all clocks seem to have 1 ns resolution, so, default is to
    // use the CLOCK_REALTIME, so throughput (nops/sec) is metris's calculation
    // is consistent with normal expectation.
    int clock_id = CLOCK_REALTIME;

    char *outfile = NULL;

    // Arg-parsing is only supported on Linux.
    int rv = parse_arguments(argc, argv, &clock_id, &outfile);
    if (rv) {
        errExit("Argument error.");
    }

    /* Create server message queue */

    serverId = msgget(SERVER_KEY,
                      (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IWGRP));
    if (serverId == -1) {
        errExit("msgget SERVER_KEY");
    }

    /* Establish SIGCHLD handler to reap terminated children */

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = grimReaper;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        errExit("sigaction SIGCHLD");
    }

    char run_descr[80];

    // Initialize L3-Logging
#if L3_ENABLED
    char *l3_log_mode = "<unknown>";
    const char *logfile = "/tmp/l3.c-server-test.dat";
    l3_log_t    logtype = L3_LOG_DEFAULT;

#if L3_LOGT_FPRINTF
    logtype = L3_LOG_FPRINTF;
#endif

    int e = l3_log_init(logtype, logfile);
    if (e) {
        errExit("l3_log_init");
    }

#if L3_FASTLOG_ENABLED
    l3_log_mode = "fast ";
#elif L3_LOGT_FPRINTF
    l3_log_mode = "fprintf() ";
#else
    l3_log_mode = "";
#endif  // L3_FASTLOG_ENABLED

// In build -D L3_LOC_ELF_ENABLED and -D L3_LOC_ENABLED are both ON.
// So, check in this order.
// Info-message to track how L3-logging is being done by server.

#if L3_LOC_ELF_ENABLED
    const char *loc_scheme = "LOC-ELF";
#elif L3_LOC_ENABLED
    const char *loc_scheme = "default LOC";
#else
    const char *loc_scheme = "(no LOC)";
#endif  // L3_LOC_ELF_ENCODING

    printf("Start Server, using clock '%s'"
           ": Initiate L3-%slogging to log-file '%s'"
           ", using %s encoding scheme.\n",
           clock_name(clock_id), l3_log_mode, logfile, loc_scheme);

    snprintf(run_descr, sizeof(run_descr), "L3-%slogging %s",
             l3_log_mode, loc_scheme);

#else // L3_ENABLED

    printf("Start Server, using clock ID=%d '%s': No logging.\n",
           clock_id, clock_name(clock_id));
    snprintf(run_descr, sizeof(run_descr), "Baseline - No logging");

#endif // L3_ENABLED

    /* Read requests, handle each in a separate child process */
    requestMsg req;
    responseMsg resp;

    struct timespec ts0 = {0};
    struct timespec ts1 = {0};
    if (clock_gettime(clock_id, &ts0)) {    // ***** Timing begins
        errExit("clock_gettime-ts0");
    }
    Client_info *clientp = NULL;
    for (;;) {

        msgLen = msgrcv(serverId, &req, REQ_MSG_SIZE, 0, 0);
        if (msgLen == -1) {
            if (errno == EINTR) {       /* Interrupted by SIGCHLD handler? */
                continue;               /* ... then restart msgrcv() */
            }
            errMsg("msgrcv");           /* Some other error */
            break;                      /* ... so terminate loop */
        }

        switch (req.mtype) {

          case REQ_MT_INIT:
            resp.mtype = req.mtype;         // Confirm initialization is done
            resp.clientId = req.clientId;   // For this client

            // Use the next free slot w/o trying to recycle existing free slots.
            resp.client_idx = NumActiveClientsHWM;
            resp.counter = req.counter;     // Just init'ed; no incr/decr done

            // Save off client's initial state
            clientp = &ActiveClients[NumActiveClientsHWM];
            memset(clientp, 0, sizeof(*clientp));

            clientp->clientId   = req.clientId;
            clientp->client_idx = NumActiveClientsHWM;
            clientp->client_ctr = req.counter;
            clientp->last_mtype = req.mtype;

            NumActiveClients++;
            NumActiveClientsHWM++;
            printf("Server: Client ID=%d joined. Clock ID=%d, # active clients=%d (HWM=%d)\n",
                   req.clientId, clock_id, NumActiveClients, NumActiveClientsHWM);
            break;

          case REQ_MT_INCR:

            clientp = &ActiveClients[req.client_idx];
            assert(clientp->clientId == req.clientId);

            resp.mtype = REQ_MT_INCR;               // Construct response message
            resp.clientId = req.clientId;           // For this client
            resp.client_idx = req.client_idx;

            clientp->last_mtype = REQ_MT_INCR;      // Prepare to increment
            resp.counter = ++clientp->client_ctr;   // Do Increment
            clientp->num_ops++;                     // Track # of operations

#if L3_ENABLED

            // Record new-counter value, to show that something can be logged.
#  if L3_FASTLOG_ENABLED
            l3_log_fast("Server msg: Increment: ClientID=%d, Counter=%" PRIu64
                        ". (L3-fast-log)",
                        resp.clientId, resp.counter);
#  else
            l3_log("Server msg: Increment: ClientID=%d, Counter=%" PRIu64 ".",
                   resp.clientId, resp.counter);
#  endif

#endif // L3_ENABLED

            break;

          case REQ_MT_SET_THROUGHPUT:
            // Record client-side computed avg throughput
            clientp = &ActiveClients[req.client_idx];
            clientp->throughput = req.counter;
            req.clientId = 0;           // Client is not interested in a response
            break;

          case REQ_MT_EXIT:

            clientp = &ActiveClients[req.client_idx];
            // One client has informed that it has exited
            NumActiveClients--;

            // NOTE: If server is started using one of the --clock arguments,
            // the metric's name logged in L3's log-line, above, will be
            // misleading. It needs to be hard-coded to a literal string at
            // compile-time. The metric's name given below is the right one
            // corresponding to the active clock.
            size_t throughput = 0;

            printf("Server: Client ID=%d exited. num_ops=%" PRIu64 " (%s)"
                   ", cumu_time_ns=%" PRIu64 " (%s ns)"
                   " (clock_id=%d)"
                   ", Avg. %s time=%" PRIu64 " ns/msg"
                   ", Server-throughput=%lu (%s) ops/sec"
                   ", # active clients=%d\n",
                   req.clientId,
                   clientp->num_ops, value_str(clientp->num_ops),
                   clientp->cumu_time_ns, value_str(clientp->cumu_time_ns),
                   clock_id, time_metric_name(clock_id),
                   (clientp->cumu_time_ns / clientp->num_ops),
                   throughput, value_str(throughput),
                   NumActiveClients);

            // Client has exited, so no need to send ack-response back to it.
            req.clientId = 0;
            if (NumActiveClients == 0) {
                goto end_forever_loop;
            }
            break;

          case REQ_MT_QUIT:
          default:
            assert(1 == 0);
            break;
        }
        // Client may have exited, so no need to send ack-response back to it.
        if (req.clientId && msgsnd(req.clientId, &resp, RESP_MSG_SIZE, 0) == -1) {
            printf("Warning: msgsnd() to client ID=%d failed to deliver.\n",
                   req.clientId);
            break;
        }

        /* Parent loops to receive next client request */
    }

end_forever_loop:
    if (clock_gettime(clock_id, &ts1)) {        // **** Timing ends
        errExit("clock_gettime-ts1");
    }
    assert(NumActiveClients == 0);

    uint64_t nsec0 = timespec_to_ns(&ts0);
    uint64_t nsec1 = timespec_to_ns(&ts1);
    uint64_t elapsed_ns = (nsec1 - nsec0);

    if (msgctl(serverId, IPC_RMID, NULL) == -1) {
        errExit("msgctl serverId");
    }
    printf("Server: # active clients=%d (HWM=%d). Exiting.\n",
           NumActiveClients, NumActiveClientsHWM);

    printSummaryStats(outfile, run_descr, ActiveClients, NumActiveClientsHWM,
                      clock_id, elapsed_ns);

    // For visibility into how clocks are performing on user's machine,
    // run clock-calibration after all workload / metrics collection is done.
#if !defined(L3_ENABLED)
    svr_clock_calibrate();
#endif  // L3_ENABLED

    exit(EXIT_SUCCESS);
}

/**
 * Helper methods
 */

/**
 * Simple argument parsing support.
 */
int
parse_arguments(const int argc, char *argv[], int *clock_id, char **outfile)
{
    int option_index = 0;
    int opt;

    while ((opt = getopt_long(argc, argv, Options_str,  Long_options, &option_index))
                != -1) {
        switch (opt) {
            case 'h':
                print_usage((const char *) argv[0], Long_options);
                exit(EXIT_SUCCESS);

            case ':':
                printf("%s: Option '%c' requires an argument\n",
                       argv[0], optopt);
                return EXIT_FAILURE;

            case 'd':   // Default clock
            case 'r':
                *clock_id = CLOCK_REALTIME;
                break;

            case 'm':
                *clock_id = CLOCK_MONOTONIC;
                break;

            case 'p':
                *clock_id = CLOCK_PROCESS_CPUTIME_ID;
                break;

            case 't':
                *clock_id = CLOCK_THREAD_CPUTIME_ID;
                break;

            // --options that need an argument
            case 'o':
                *outfile = optarg;
                break;

            case '?': // Invalid option or missing argument
                printf("%s: Invalid option '%c' or missing argument\n",
                       argv[0], opt);
                return EXIT_FAILURE;

            default:
                // Handle error
                return EXIT_FAILURE;
        }
    }
    return 0;
}

void
print_usage(const char *program_name, struct option options[])
{
    printf("Usage: %s [options] "
           "{-p | --program-binary} <program-binary> [ <loc-IDs>+ ]\n",
           program_name);
    printf("Options:\n");

    for (int i = 0; options[i].name != NULL; i++) {
        if (options[i].val != 0) {
            printf("  -%c, --%s", options[i].val, options[i].name);
        } else {
            printf("      --%s", options[i].name);
        }

        if (options[i].has_arg == required_argument) {
            printf(" <%s>", options[i].name);
        } else if (options[i].has_arg == optional_argument) {
            printf(" [<%s>]", options[i].name);
        }
        printf("\n");
    }
}

#define NUM_ITERATIONS 1000000

/**
 * Calibrate the clocks available on this host.
 * Find the resolution of the clock used in the program's timing measurement.
 * Report avg. 'estimated' overhead of making a clock_gettime() call.
 */
void
svr_clock_calibrate(void)
{
    clockid_t clockids[] = {  CLOCK_REALTIME
                            , CLOCK_MONOTONIC
                            , CLOCK_PROCESS_CPUTIME_ID
                            , CLOCK_THREAD_CPUTIME_ID
                           };

    printf("Calibrate clock overheads over %d (%s) iterations:\n",
           NUM_ITERATIONS, value_str(NUM_ITERATIONS));
    for (int cctr = 0; cctr < ARRAY_LEN(clockids); cctr++) {
        clockid_t clockid = clockids[cctr];

        struct timespec ts_clock;
        if (clock_getres(clockid, &ts_clock)) {
            errExit("clock_getres-Calibrate");
        }
        unsigned ovhd = svr_clock_overhead(clockid);
        printf("Average overhead for clock_id=%d (%s): %u ns"
               ", Resolution = %" PRIu64 " ns\n",
               clockid, clock_name(clockid), ovhd,
               timespec_to_ns(&ts_clock));

    }
}

/**
 * Helper method to calibrate m/c-specific clock-overheads for a specific clock.
 * Gives us an idea of how-much impact the call to clock_gettime() itself has
 * on the overall 'measured' elapsed-time.
 *
 * Returns: Overhead estimated in ns.
 */
unsigned
svr_clock_overhead(clockid_t clock_id)
{
    struct timespec start;
    struct timespec end;
    size_t          total_time_ns = 0;
    uint32_t        nops = 0;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        clock_gettime(clock_id, &start);
        clock_gettime(clock_id, &end);
        int delta_ns = (timespec_to_ns(&end) - timespec_to_ns(&start));
        // Account for aberrations that might return negative time-deltas.
        if (delta_ns > 0) {
            total_time_ns += delta_ns;
            nops++;
        }
    }
    return (total_time_ns / nops);
}

// Return hard-code string of clock-name given clock-ID
// We need this and time_metric_name() as the clock_id mnemonics are
// not sequential on Mac/OSX, which does not allow use of string lookup
// tables. So, need to do some remapping.
const char *
clock_name(int clock_id)
{
    switch (clock_id) {
      case CLOCK_REALTIME:
        return "CLOCK_REALTIME";

      case CLOCK_MONOTONIC:
        return "CLOCK_MONOTONIC";

      case CLOCK_PROCESS_CPUTIME_ID:
        return "CLOCK_PROCESS_CPUTIME_ID";

      case CLOCK_THREAD_CPUTIME_ID:
        return "CLOCK_THREAD_CPUTIME_ID";
    }
    return "CLOCK_UNKNOWN";
}

// Return hard-code string of semantic of time tracked by given clock-ID
const char *
time_metric_name(int clock_id)
{
    switch (clock_id) {
      case CLOCK_REALTIME:
        return "Elapsed real";

      case CLOCK_MONOTONIC:
        return "Monotonic";

      case CLOCK_PROCESS_CPUTIME_ID:
        return "Process-CPU";

      case CLOCK_THREAD_CPUTIME_ID:
        return "Thread-CPU";
    }
    return "unknown";
}

/**
 * printSummaryStats() - Aggregate metrics and print summary across all clients.
 */
void
printSummaryStats(const char *outfile, const char *run_descr,
                  Client_info *clients, unsigned int num_clients,
                  int clock_id, uint64_t elapsed_ns)
{
    size_t  num_ops = 0;
    size_t  sum_throughput = 0;

    for (int cctr = 0 ; cctr < num_clients; cctr++) {
        num_ops += clients[cctr].num_ops;
        sum_throughput += clients[cctr].throughput;
    }
    size_t svr_throughput = (uint64_t) ((num_ops * 1.0 / elapsed_ns)
                                            * L3_NS_IN_SEC);
    size_t cli_throughput = (sum_throughput / num_clients);

    printf("For %u clients, %s, num_ops=%lu (%s) ops"
           ", Elapsed time=%" PRIu64 " (%s) ns"
           ", Avg. %s time=%" PRIu64 " ns/msg"
           ", Server throughput=%lu (%s) ops/sec"
           ", Client throughput=%lu (%s) ops/sec"
           "\n",
           num_clients, run_descr, num_ops, value_str(num_ops),
           elapsed_ns, value_str(elapsed_ns),
           time_metric_name(clock_id), (elapsed_ns / num_ops),
           svr_throughput, value_str(svr_throughput),
           cli_throughput, value_str(cli_throughput));

    // Generate one-line summary for post-processing by perf-report.py
    if (outfile) {
        printf("tail -f %s\n", outfile);
        FILE *fh = fopen(outfile, "a");
        if (!fh) {
            printf("Error! Unable to open perf-metrics output file '%s\n",
                   outfile);
            exit(EXIT_FAILURE);
        }
        fprintf(fh, "%s, NumClients=%u, NumOps=%lu (%s)"
                    ", Server throughput=%lu (%s) ops/sec"
                    ", Client throughput=%lu (%s) ops/sec"
                    ", elapsed_ns=%" PRIu64 " (%s) ns\n",
                run_descr, num_clients, num_ops, value_str(num_ops),
                svr_throughput, value_str(svr_throughput),
                cli_throughput, value_str(cli_throughput),
                elapsed_ns, value_str(elapsed_ns));
        fclose(fh);
    }
}
