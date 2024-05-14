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
       ./svmsg_file_server [ --clock-monotonic
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
 */
typedef struct client_info {
    int             clientId;
    int             client_idx;     // Into ActiveClients[] array, below
    int64_t         client_ctr;     // Current value of client's counter
    uint64_t        cumu_time_ns;   // Cumulative time for operation.
    uint64_t        num_ops;        // # of msg-op requests received
    req_resp_type_t last_mtype;     // Last requests->mtype
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
      { "help"                      , no_argument         , NULL, 'h'}
    , { "clock-monotonic"           , no_argument         , NULL, 'm'}
    , { "clock-process-cputime-id"  , no_argument         , NULL, 'p'}
    , { "clock-realtime"            , no_argument         , NULL, 'r'}
    , { "clock-thread-cputime-id"   , no_argument         , NULL, 't'}
    , { NULL, 0, NULL, 0}           // End of options
};

const char * Options_str = ":hrmptd";

// Function Prototypes

int parse_arguments(const int argc, char *argv[], int *clock_id);
void print_usage(const char *program, struct option options[]);
void printSummaryStats(Client_info *clients, unsigned int num_clients,
                       int clock_id);

void svr_clock_getres(void);
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

    // On MacOSX, only the CLOCK_THREAD_CPUTIME_ID clock has a resolution of 1ns.
    // On Linux, all clocks seem to have 1 ns resolution, so, default is to also
    // use the same CLOCK_THREAD_CPUTIME_ID.
    int clock_id;

#if __APPLE__
    printf("%s is currently not supported on Mac/OSX\n", argv[0]);
    exit(EXIT_SUCCESS);
#endif

    clock_id = CLOCK_THREAD_CPUTIME_ID;

    // Arg-parsing is only supported on Linux.
    int rv = parse_arguments(argc, argv, &clock_id);
    if (rv) {
        errExit("Argument error.");
    }

    svr_clock_getres();

    /* Create server message queue */

    serverId = msgget(SERVER_KEY,
                      (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IWGRP));
    if (serverId == -1) {
        errExit("msgget");
    }

    /* Establish SIGCHLD handler to reap terminated children */

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = grimReaper;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        errExit("sigaction");
    }

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
        errExit("l3_init");
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
           ", using logtype '%s', %s encoding scheme.\n",
           clock_name(clock_id), l3_log_mode, logfile,
           l3_logtype_name(logtype), loc_scheme);

#else // L3_ENABLED

    printf("Start Server, using clock ID=%d '%s': No logging.\n",
           clock_id, clock_name(clock_id));

#endif // L3_ENABLED

    /* Read requests, handle each in a separate child process */
    requestMsg req;
    responseMsg resp;

    for (;;) {

        msgLen = msgrcv(serverId, &req, REQ_MSG_SIZE, 0, 0);
        if (msgLen == -1) {
            if (errno == EINTR) {       /* Interrupted by SIGCHLD handler? */
                continue;               /* ... then restart msgrcv() */
            }
            errMsg("msgrcv");           /* Some other error */
            break;                      /* ... so terminate loop */
        }

        Client_info *clientp = NULL;

        // To track elapsed-time measurement for block of case: body
        struct timespec ts0;
        struct timespec ts1;
        uint64_t nsec0 = 0;
        uint64_t nsec1 = 0;
        uint64_t elapsed_ns = 0;

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
            if (clock_gettime(clock_id, &ts0)) {    // Timing begins
                errExit("clock_gettime-ts0");
            }

            clientp = &ActiveClients[req.client_idx];
            assert(clientp->clientId == req.clientId);

            resp.mtype = REQ_MT_INCR;               // Construct response message
            resp.clientId = req.clientId;           // For this client
            resp.client_idx = req.client_idx;

            clientp->last_mtype = REQ_MT_INCR;      // Prepare to increment
            resp.counter = ++clientp->client_ctr;   // Do Increment
            clientp->num_ops++;                     // Track # of operations

            if (clock_gettime(clock_id, &ts1)) {    // Timing ends
                errExit("clock_gettime-ts1");
            }

            nsec0 = timespec_to_ns(&ts0);
            nsec1 = timespec_to_ns(&ts1);
            elapsed_ns = (nsec1 - nsec0);           // Elapsed-ns for this op

#if L3_ENABLED

            // Record time-consumed. (NOTE: See clarification below.)
#  if L3_FASTLOG_ENABLED
            l3_log_fast("Server msg: Increment: ClientID=%d, "
                        "Thread-CPU-time=%" PRIu64 " ns. (L3-fast-log)",
                        resp.clientId, elapsed_ns);
#  else
            l3_log_simple("Server msg: Increment: ClientID=%d, "
                          "Thread-CPU-time=%" PRIu64 " ns.\n",
                          resp.clientId, elapsed_ns);
#  endif

#endif // L3_ENABLED

            // Reuse variable to reacquire current TS for accumulation.
            // Cumulative time-consumed gives a read into impact of any
            // logging call that might be done in diff micro-benchmarks.

            if (clock_gettime(clock_id, &ts1)) {    // Cumulative timing ends
                errExit("clock_gettime-ts1");
            }
            nsec1 = timespec_to_ns(&ts1);
            elapsed_ns = (nsec1 - nsec0);
            clientp->cumu_time_ns += elapsed_ns;    // Cumulative elapsed-ns

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
            size_t throughput = (uint64_t)
                        ((clientp->num_ops * 1.0 / clientp->cumu_time_ns)
                                    * L3_NS_IN_SEC);

            printf("Server: Client ID=%d exited. num_ops=%" PRIu64 " (%s)"
                   ", cumu_time_ns=%" PRIu64
                   " (clock_id=%d)"
                   ", Avg. %s time=%" PRIu64 " ns/msg"
                   ", Server-throughput=%lu (%s) ops/sec"
                   ", # active clients=%d\n",
                   req.clientId,
                   clientp->num_ops, value_str(clientp->num_ops),
                   clientp->cumu_time_ns,
                   clock_id, time_metric_name(clock_id),
                   (clientp->cumu_time_ns / clientp->num_ops),
                   throughput, value_str(throughput),
                   NumActiveClients);

            if (NumActiveClients == 0) {
                break;
            }
            // Client has exited, so no need to send ack-response back to it.
            req.clientId = 0;
            break;

          case REQ_MT_DECR:
          case REQ_MT_GET_CTR:
          case REQ_MT_QUIT:
          default:
            assert(1 == 0);
            break;
        }
        // Client may have exited, so no need to send ack-response back to it.
        if (req.clientId && msgsnd(req.clientId, &resp, RESP_MSG_SIZE, 0) == -1) {
            break;
        }

        /* Parent loops to receive next client request */
    }
    assert(NumActiveClients == 0);

    if (msgctl(serverId, IPC_RMID, NULL) == -1) {
        errExit("msgctl");
    }
    printf("Server: # active clients=%d (HWM=%d). Exiting.\n",
           NumActiveClients, NumActiveClientsHWM);

    printSummaryStats(ActiveClients, NumActiveClientsHWM, clock_id);

    exit(EXIT_SUCCESS);
}

/**
 * Helper methods
 */

/**
 * Simple argument parsing support.
 */
int
parse_arguments(const int argc, char *argv[], int *clock_id)
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

// Find the resolution of the clock used in the program's timing measurement.
void
svr_clock_getres(void)
{
    struct timespec ts_real;
    if (clock_getres(CLOCK_REALTIME, &ts_real)) {
        errExit("clock_getres-ts_real");
    }

    struct timespec ts_monotonic;
    if (clock_getres(CLOCK_MONOTONIC, &ts_monotonic)) {
        errExit("clock_getres-ts_monotonic");
    }

    struct timespec ts_processCPU;
    if (clock_getres(CLOCK_PROCESS_CPUTIME_ID, &ts_processCPU)) {
        errExit("clock_getres-ts_processCPU");
    }

    struct timespec ts_threadCPU;
    if (clock_getres(CLOCK_THREAD_CPUTIME_ID, &ts_threadCPU)) {
        errExit("clock_getres-ts_threadCPU");
    }
    printf("# Debug info:"
           " Server clocks resolution"
           ": CLOCK_REALTIME [id=%d] = %" PRIu64 " ns"
           ", CLOCK_MONOTONIC [id=%d] = %" PRIu64 " ns"
           ", CLOCK_PROCESS_CPUTIME_ID [id=%d] = %" PRIu64 " ns"
           ", CLOCK_THREAD_CPUTIME_ID [id=%d] = %" PRIu64 " ns"
           "\n\n",
           CLOCK_REALTIME, timespec_to_ns(&ts_real),
           CLOCK_MONOTONIC, timespec_to_ns(&ts_monotonic),
           CLOCK_PROCESS_CPUTIME_ID, timespec_to_ns(&ts_processCPU),
           CLOCK_THREAD_CPUTIME_ID, timespec_to_ns(&ts_threadCPU));
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
printSummaryStats(Client_info *clients, unsigned int num_clients,
                  int clock_id)
{
    size_t  num_ops = 0;
    size_t  cumu_time_ns = 0;

    for (int cctr = 0 ; cctr < num_clients; cctr++) {
        num_ops += clients[cctr].num_ops;
        cumu_time_ns += clients[cctr].cumu_time_ns;
    }
    size_t throughput = (uint64_t) ((num_ops * 1.0 / cumu_time_ns)
                                        * L3_NS_IN_SEC);
    printf("For %u clients, num_ops=%lu (%s) ops"
           ", Avg. %s time=%lu ns/msg"
           ", throughput=%lu (%s) ops/sec"
           "\n",
           num_clients, num_ops, value_str(num_ops),
           time_metric_name(clock_id), (cumu_time_ns / num_ops),
           throughput, value_str(throughput));
}
