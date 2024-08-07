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
#include <pthread.h>

#ifdef __cplusplus
#  if defined(L3_LOGT_SPDLOG) or defined(L3_LOGT_SPDLOG_BACKTRACE)

#   include <iostream>
#   include <atomic>
#   include "spdlog/spdlog.h"
#   include "spdlog/sinks/basic_file_sink.h"
    using namespace std;

#  endif    // L3_LOGT_SPDLOG
#endif  // __cplusplus

#include "svmsg_file.h"
#include "l3.h"
#include "size_str.h"

/**
 * Global data structures to track client-specific information.
 *
 * NOTE: This program can be configured to use different clocks to measure
 * time, using the --clock<arg> argument.
 */
typedef struct client_info {
    int             clientId;
    int             client_idx;         // Into ActiveClients[] array, below
    int64_t         client_ctr;         // Current value of client's counter
    uint64_t        num_ops;            // # of msg-op requests received
    uint64_t        throughput;         // Client-side avg throughput nops/sec
    req_resp_type_t last_mtype;         // Last request's->mtype
} Client_info;

Client_info ActiveClients[MAX_CLIENTS];

/*
 * Configuration for each server thread.
 */
typedef struct {
    int         server_id;  // Server's msgq-ID.

    // On Mac/OSX, pthread_self() returns a struct, not an ID. So, to get
    // code compiling, print the thread's index in the threads[] array.
    int         svr_thread_idx;
    uint64_t    svr_num_ops;    // Processed by this thread

} svr_thread_config;

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
#ifdef __cplusplus
static std::atomic<int> NumActiveClientsHWM{0};
static std::atomic<int> NumActiveClients{0};
#else   // __cplusplus
static _Atomic int NumActiveClientsHWM = 0;
static _Atomic int NumActiveClients    = 0;
#endif  // __cplusplus

/**
 * On MacOSX, only the CLOCK_THREAD_CPUTIME_ID clock has a resolution of 1ns.
 * On Linux, all clocks seem to have 1 ns resolution, so use the CLOCK_REALTIME
 * clock as the default clock to use. This way, the throughput metric computed
 * as (nops/sec) is consistent with normal expectation of 'sec' as real time
 * seconds.
 */
int Clock_id = CLOCK_REALTIME;


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
    , { "num-server-threads"        , required_argument   , NULL, 'n'}
    , { "perf-outfile"              , required_argument   , NULL, 'o'}
    , { NULL, 0, NULL, 0}           // End of options
};

const char * Options_str = "no:dhmprt";

// Useful macros
#define ARRAY_LEN(arr)  (sizeof(arr) / sizeof(*arr))

#if defined(L3_LOGT_SPDLOG)
std::shared_ptr<spdlog::logger> Spd_logger; // = spdlog::basic_logger_mt();
#endif  // L3_LOGT_SPDLOG


// Function Prototypes

// Server-method prototypes

int svr_start_threads(pthread_t *thread_ids, int nthreads, int serverId,
                      svr_thread_config * svr_config);

void *svr_proc_process_msg(void *cfg);
int svr_op_ct_init(requestMsg *req);
int svr_op_incr(requestMsg *req);

int parse_arguments(const int argc, char *argv[], int *clock_id,
                    char **outfile, int *num_threads);
void print_usage(const char *program, struct option options[]);

void printSummaryStats(const char *outfile, const char *run_descr,
                       Client_info *clients, unsigned int num_clients,
                       int clock_id, uint64_t elapsed_ns,
                       svr_thread_config * svr_config, int num_threads);

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
    struct sigaction sa;

#if __APPLE__
    printf("%s is currently not supported on Mac/OSX\n", argv[0]);
    exit(EXIT_SUCCESS);
#endif

    char *outfile = NULL;
    int num_threads = 1;

    // Arg-parsing is only supported on Linux.
    int rv = parse_arguments(argc, argv, &Clock_id, &outfile, &num_threads);
    if (rv) {
        errExit("Argument error.");
    }

    /* Create server message queue */

    int serverId = msgget(SERVER_KEY,
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

    // L3 does not directly support spdlog. Manaage it separately.
#if defined(L3_LOGT_SPDLOG)
    // Create basic file logger (not rotated).
    const char *logfile = "/tmp/l3.c-server-test.dat";
    Spd_logger = spdlog::basic_logger_mt("file_logger", logfile, true);
    const char *logging_type = "spdlog";

    printf("\n**** Start Server, using clock '%s'"
           ": Initiate spdlog-logging to log-file '%s'"
           ", using logtype '%s'.\n",
           clock_name(Clock_id), logfile, logging_type);

    snprintf(run_descr, sizeof(run_descr), "%s-logging", logging_type);

#elif defined(L3_LOGT_SPDLOG_BACKTRACE)

    const char *logfile = "/tmp/l3.c-server-test.dat";
    // Configure same # of backtrace msg-capacity to spdlog as
    // is being configured for L3's mmap()-ed buffer.
    spdlog::enable_backtrace(L3_MAX_SLOTS);

    const char *logging_type = "spdlog-backtrace";

    printf("\n**** Start Server, using clock '%s'"
           ": Initiate spdlog-logging (log-file '%s' unused)"
           ", using logtype '%s'.\n",
           clock_name(Clock_id), logfile, logging_type);

    snprintf(run_descr, sizeof(run_descr), "%s-logging", logging_type);

#elif L3_ENABLED
    // Initialize L3-Logging
    const char *l3_log_mode = "<unknown>";
    const char *logfile = "/tmp/l3.c-server-test.dat";
    l3_log_t    logtype = L3_LOG_DEFAULT;

#if L3_LOGT_FPRINTF
    logtype = L3_LOG_FPRINTF;
#elif L3_LOGT_WRITE
    logtype = L3_LOG_WRITE;
#endif

    int e = l3_log_init(logtype, logfile);
    if (e) {
        errExit("l3_log_init");
    }

#if L3_FASTLOG_ENABLED
    l3_log_mode = "fast ";
#elif L3_LOGT_FPRINTF
    l3_log_mode = "fprintf() ";
#elif L3_LOGT_WRITE
    l3_log_mode = "write() ";
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

    printf("\n**** Start Server, using clock '%s'"
           ": Initiate L3-%slogging to log-file '%s'"
           ", using %s encoding scheme.\n",
           clock_name(Clock_id), l3_log_mode, logfile, loc_scheme);

    snprintf(run_descr, sizeof(run_descr), "L3-%slogging %s",
             l3_log_mode, loc_scheme);

#else // L3_ENABLED

    printf("\n**** Start Server, using clock ID=%d '%s': No logging"
           ", %d server-threads, server-msgq-ID=%d.\n",
           Clock_id, clock_name(Clock_id), num_threads, serverId);
    snprintf(run_descr, sizeof(run_descr), "Baseline - No logging");

#endif // L3_ENABLED

    // Invoke pthread_create() to create n-threads ...
    svr_thread_config   svr_config[num_threads];
    pthread_t   thread_ids[num_threads];

    struct timespec ts0 = {0};

    if (svr_start_threads(thread_ids, num_threads, serverId, svr_config)) {
        errExit("Server thread creation failed.");
    }
#if !defined(__APPLE__)
    if (clock_gettime(Clock_id, &ts0)) {        // ***** Timing begins
        errExit("clock_gettime-ts0");
    }
#endif  // ! __APPLE__

   // Wait for all threads to complete ...
   for (int tctr = 0; tctr < num_threads; tctr++) {
      void *thread_rc;
      int   rc = pthread_join(thread_ids[tctr], &thread_rc);
      if (rc) {
        errExit("pthread_join() failed.");
      }
    }

    struct timespec ts1 = {0};

#if !defined(__APPLE__)
    if (clock_gettime(Clock_id, &ts1)) {        // **** Timing ends
        errExit("clock_gettime-ts1");
    }
#endif  // ! __APPLE__

    assert(NumActiveClients == 0);

    uint64_t nsec0 = timespec_to_ns(&ts0);
    uint64_t nsec1 = timespec_to_ns(&ts1);
    uint64_t elapsed_ns = (nsec1 - nsec0);

    if (msgctl(serverId, IPC_RMID, NULL) == -1) {
        errExit("msgctl serverId");
    }

    // Close L3-logging upon exit.
#if defined(L3_LOGT_SPDLOG_BACKTRACE)

    // Log the messages now! show the last n-messages
    spdlog::dump_backtrace();

#elif L3_ENABLED

    l3_log_deinit(logtype);

#endif  // L3_ENABLED

#ifdef __cplusplus
    int activeClients = NumActiveClients.load();
    int activeClientsHWM = NumActiveClientsHWM.load();
#else
    int activeClients = NumActiveClients;
    int activeClientsHWM = NumActiveClientsHWM;
#endif
    printf("Server: # active clients=%d (HWM=%d). Exiting.\n",
           activeClients, activeClientsHWM);

    printSummaryStats(outfile, run_descr, ActiveClients, NumActiveClientsHWM,
                      Clock_id, elapsed_ns,
                      svr_config, num_threads);

    // For visibility into how clocks are performing on user's machine,
    // run clock-calibration after all workload / metrics collection is done.
    // One-time calibration (done when # server-threads==1) is sufficient.

#if defined(__cplusplus)

#if !defined(L3_ENABLED) and !defined(L3_LOGT_SPDLOG) and !defined(L3_LOGT_SPDLOG_BACKTRACE)
    if (num_threads == 1) {
        svr_clock_calibrate();
    }
#endif

#else   // __cplusplus

#if !defined(L3_ENABLED)
    if (num_threads == 1) {
        svr_clock_calibrate();
    }
#endif  // L3_ENABLED

#endif  // __cplusplus

    exit(EXIT_SUCCESS);
}

/**
 * *****************************************************************************
 * Server methods to implement each operation.
 * *****************************************************************************
 */
/**
 * -----------------------------------------------------------------------------
 * svr_start_threads() - Start 'n' server-threads, receiving and processing
 * messages from clients.
 *
 * Parameters:
 *  thread_ids      - Array of 'nthreads' thread-IDs
 *  nthreads        - Number of threads to start
 *  serverId        - Server's msgq-ID
 *  svr_config      - Array of thread's config struct (Used to output metrics)
 *
 * Returns:
 *  0 => Created required # of threads successfully.
 *  Non-zero => Some error(s) during thread creation.
 * -----------------------------------------------------------------------------
 */
int
svr_start_threads(pthread_t *thread_ids, int nthreads, int serverId,
                  svr_thread_config *svr_config)
{
    int rv = 0;
    for (int tctr = 0; tctr < nthreads; tctr++) {
        svr_config[tctr].server_id = serverId;
        svr_config[tctr].svr_thread_idx = tctr;
        svr_config[tctr].svr_num_ops = 0;
        rv = pthread_create(&thread_ids[tctr], NULL, svr_proc_process_msg,
                            (void *) &svr_config[tctr]);
        if (rv) {
            break;
        } else {
            printf("Started server thread ID = %" PRIu64 "\n",
#if __APPLE__
                   (uint64_t) tctr
#else
                   thread_ids[tctr]
#endif  // __APPLE__
                  );
        }
    }
    return rv;
}

/**
 * -----------------------------------------------------------------------------
 * svr_proc_process_msg() - for()-ever loop to process messages from clients.
 * -----------------------------------------------------------------------------
 */
void *
svr_proc_process_msg(void *cfg)
{
    // Unpack the thread's config-params to get a hold of server's msgq-ID
    svr_thread_config *svr_config = (svr_thread_config *) cfg;
    int serverId = svr_config->server_id;

    ssize_t msgLen;
    Client_info *clientp = NULL;
    requestMsg req;

#if __APPLE__
    uint64_t tid = svr_config->svr_thread_idx;
#else
    pthread_t tid = pthread_self();
#endif  // __APPLE__

    uint64_t    svr_num_ops = 0;
    for (;;) {

        if (NumActiveClientsHWM && (NumActiveClients == 0)) {
            goto end_forever_loop;
        }

        msgLen = msgrcv(serverId, &req, REQ_MSG_SIZE, 0, 0);
        if (msgLen == -1) {
            if (errno == EINTR) {       /* Interrupted by SIGCHLD handler? */
                continue;               /* ... then restart msgrcv() */
            }
            errMsg("msgrcv");           /* Some other error */
            break;                      /* ... so terminate loop */
        }

        switch ((int) req.mtype) {

          case REQ_MT_INIT:
            if (svr_op_ct_init(&req)) {
                errExit("svr_op_ct_init() failed.");
            }
            break;

          case REQ_MT_INCR:
            if (svr_op_incr(&req)) {
                errExit("svr_op_incr() failed.");
            }
            svr_num_ops++;  // Just count the actual messages needing some action.
            break;

          case REQ_MT_SET_THROUGHPUT:
            // Record client-side computed avg throughput
            clientp = &ActiveClients[req.client_idx];
            clientp->throughput = req.counter;
            break;

          case REQ_MT_EXIT:

            // -ve client-index => it's a server-thread informing us to exit.
            if (req.client_idx >= 0) {
                clientp = &ActiveClients[req.client_idx];
                // One client has informed that it has exited
                NumActiveClients--;

#ifdef __cplusplus
                int activeClients = NumActiveClients.load();
#else
                int activeClients = NumActiveClients;
#endif
                printf("Server: Client ID=%d exited. num_ops=%" PRIu64 " (%s)"
                       " # active clients=%d\n",
                       req.clientId,
                       clientp->num_ops, value_str(clientp->num_ops),
                       activeClients);
            }

            if (NumActiveClients <= 0) {
                printf("Server: ThreadID=%" PRIu64 " exiting.\n", tid);

                if (NumActiveClients == 0) {
                    req.mtype = REQ_MT_EXIT;

                    // Inform last remaining server-thread that this is
                    // a final-exit from a server-thread, and not from a real
                    // client.
                    req.client_idx = -1;
                    if (msgsnd(serverId, &req, REQ_MSG_SIZE, 0) == -1) {
                        errExit("thread-msgsnd-exit");
                    }
                }
                goto end_forever_loop;
            }
            break;

#if !defined(__cplusplus)
          case REQ_MT_QUIT:
          default:
            assert(1 == 0);
            break;
#endif  // __cplusplus

        }

        /* Parent loops to receive next client request */
    }

end_forever_loop:

    svr_config->svr_num_ops = svr_num_ops;
    return 0;
}

/**
 * -----------------------------------------------------------------------------
 * REQ_MT_INIT: Client initialization
 *
 * Returns:
 *  0 => success; Non-zero => some failure.
 * -----------------------------------------------------------------------------
 */
int
svr_op_ct_init(requestMsg *req)
{
    assert(req->mtype == REQ_MT_INIT);

    responseMsg resp;
    memset(&resp, 0, sizeof(resp));

    resp.mtype = req->mtype;         // Confirm initialization is done

    assert(req->clientId > 0);
    resp.clientId = req->clientId;   // For this client

    // Use the next free slot w/o trying to recycle existing free slots.
    resp.client_idx = NumActiveClientsHWM++;
    resp.counter = req->counter;     // Just init'ed; no incr/decr done

    // Save off client's initial state
    Client_info *clientp = &ActiveClients[resp.client_idx];
    memset(clientp, 0, sizeof(*clientp));

    clientp->clientId   = req->clientId;
    clientp->client_idx = resp.client_idx;
    clientp->client_ctr = req->counter;
    clientp->last_mtype = req->mtype;

    NumActiveClients++;
#ifdef __cplusplus
    int activeClients = NumActiveClients.load();
#else
    int activeClients = NumActiveClients;
#endif
    printf("Server: Client ID=%d joined. # active clients=%d (HWM=%d)\n",
           req->clientId, activeClients, (resp.client_idx + 1));

    int rv = 0;
    if (msgsnd(req->clientId, &resp, RESP_MSG_SIZE, 0) == -1) {
        printf("Warning: msgsnd() to client ID=%d failed to deliver.\n",
               req->clientId);
        rv = -1;
    }
    return rv;
}

/**
 * -----------------------------------------------------------------------------
 * REQ_MT_INCR: Increment operation: Bump up client_ctr by 1.
 *
 * Returns:
 *  0 => success; Non-zero => some failure.
 * -----------------------------------------------------------------------------
 */
int
svr_op_incr(requestMsg *req)
{
    Client_info *clientp = &ActiveClients[req->client_idx];
    assert(clientp->clientId == req->clientId);

    responseMsg resp;
    memset(&resp, 0, sizeof(resp));

    resp.mtype = REQ_MT_INCR;               // Construct response message
    resp.clientId = req->clientId;           // For this client
    resp.client_idx = req->client_idx;

    clientp->last_mtype = REQ_MT_INCR;      // Prepare to increment
    resp.counter = ++clientp->client_ctr;   // Do Increment
    clientp->num_ops++;                     // Track # of operations

    // Record time-consumed. (NOTE: See clarification below.)
#  if defined(L3_LOGT_SPDLOG)
    Spd_logger->info("Server spdlog: "
                     "Increment: ClientID={}, Counter={}.",
                     resp.clientId, resp.counter);

#  elif defined(L3_LOGT_SPDLOG_BACKTRACE)

    spdlog::debug("Server spdlog-Backtrace: "
                 "Increment: ClientID={}, Counter={}.",
                 resp.clientId, resp.counter);

#elif L3_ENABLED

#  if L3_FASTLOG_ENABLED
    l3_log_fast("Server msg: Increment: ClientID=%d, Counter=%" PRIu64
                ". (L3-fast-log)",
                resp.clientId, resp.counter);
#  else
    l3_log("Server msg: Increment: ClientID=%d, Counter=%" PRIu64 ".\n",
           resp.clientId, resp.counter);
#  endif    // L3_FASTLOG_ENABLED

#endif // L3_ENABLED

    int rv = 0;
    if (msgsnd(req->clientId, &resp, RESP_MSG_SIZE, 0) == -1) {
        printf("Warning: msgsnd() to client ID=%d failed to deliver.\n",
               req->clientId);
        rv = -1;
    }
    return rv;
}


/**
 * -----------------------------------------------------------------------------
 * Helper methods
 * -----------------------------------------------------------------------------
 */

/**
 * Simple argument parsing support.
 */
int
parse_arguments(const int argc, char *argv[], int *clock_id,
                char **outfile, int *num_threads)
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
            case 'n':
               *num_threads = atoi(optarg);
               break;

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
    for (unsigned cctr = 0; cctr < ARRAY_LEN(clockids); cctr++) {
        clockid_t clockid = clockids[cctr];

        struct timespec ts_clock;
        if (clock_getres(clockid, &ts_clock)) {
            errExit("clock_getres-Calibrate");
        }
        unsigned ovhd = svr_clock_overhead(clockid);
        printf("Average overhead for Clock_id=%d (%s): %u ns"
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
 *
 * Parameters:
 *  outfile     - Name of logging output file (if logging is done)
 *  run_descr   - Run description string
 *  clients     - Array of num_clients Client_info structs (for metrics)
 *  num_clients - Number of clients
 *  clock_id    - Server-side Clock-ID used for elapsed-time accounting
 *  elapsed_ns  - Elapsed time, in ns
 *  svr_config  - Array of num_threads svr_thread_config structs
 *                (for server-thread specific metrics)
 *  num_threads - Number of server-threads invoked for workload
 */
void
printSummaryStats(const char *outfile, const char *run_descr,
                  Client_info *clients, unsigned int num_clients,
                  int clock_id, uint64_t elapsed_ns,
                  svr_thread_config *svr_config, int num_threads)
{
    size_t  num_ops = 0;
    size_t  sum_throughput = 0;

    int nclients_div = 0;
    for (unsigned int cctr = 0 ; cctr < num_clients; cctr++) {
        num_ops += clients[cctr].num_ops;
        // Defensive check: In case something went wrong ...
        if (clients[cctr].throughput == 0) {
            printf("\nWarning!! Throughput metric for client ID=%d is 0\n",
                   cctr);
        } else {
            sum_throughput += clients[cctr].throughput;
            nclients_div++;
        }
    }
    size_t cli_throughput = (sum_throughput / nclients_div);

    size_t svr_throughput = (uint64_t) ((num_ops * 1.0 / elapsed_ns)
                                            * L3_NS_IN_SEC);

    size_t avg_ops_per_thread = 0;
    int num_threads_div = 0;
    for (int tctr = 0; tctr < num_threads; tctr++) {
        // Defensive check: In case something went wrong ...
        if (svr_config[tctr].svr_num_ops == 0) {
            printf("\nWarning!! #-ops processsed metric for server thread=%d is 0\n",
                   tctr);
        } else {
            avg_ops_per_thread += svr_config[tctr].svr_num_ops;
            num_threads_div++;
        }
    }
    avg_ops_per_thread = (size_t) ((avg_ops_per_thread * 1.0) / num_threads_div);

    printf("\nSummary:"
           " For %u clients, %s, num_threads=%d, num_ops=%lu (%s) ops"
           ", Elapsed time=%" PRIu64 " (%s) ns"
           ", Avg. %s time=%" PRIu64 " ns/msg"
           ", Server throughput=%lu (%s) ops/sec"
           ", Client throughput=%lu (%s) ops/sec"
           ", Avg. ops per thread=%lu (%s) ops/thread"
           "\n\n",
           num_clients, run_descr, num_threads, num_ops, value_str(num_ops),
           elapsed_ns, value_str(elapsed_ns),
           time_metric_name(clock_id), (elapsed_ns / num_ops),
           svr_throughput, value_str(svr_throughput),
           cli_throughput, value_str(cli_throughput),
           avg_ops_per_thread, value_str(avg_ops_per_thread));

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
                    ", NumThreads=%d"
                    ", Server throughput=%lu (%s) ops/sec"
                    ", Client throughput=%lu (%s) ops/sec"
                    ", elapsed_ns=%" PRIu64 " (%s) ns"
                    ", NumOps/thread=%lu (%s)"
                    "\n",
                run_descr, num_clients, num_ops, value_str(num_ops),
                num_threads, svr_throughput, value_str(svr_throughput),
                cli_throughput, value_str(cli_throughput),
                elapsed_ns, value_str(elapsed_ns),
                avg_ops_per_thread, value_str(avg_ops_per_thread));
        fclose(fh);
    }
}
