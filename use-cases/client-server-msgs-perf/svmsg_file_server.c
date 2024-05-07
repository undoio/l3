/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2024.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* svmsg_file_server.c

   The server and client communicate using System V message queues.

   This version of the application has been extensively modified based
   on an initial version downloaded from:
   https://man7.org/tlpi/code/online/all_files_by_chapter.html

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
*/
#include <inttypes.h>
#include <time.h>
#include <assert.h>
#include "svmsg_file.h"
#include "l3.h"

/**
 * Global data structures to track client-specific information.
 */
typedef struct client_info {
    int             clientId;
    int             client_idx; // Into ActiveClients[] array, below
    int64_t         client_ctr; // Current value of client's counter
    uint64_t        cumu_thread_cpu_time_ns;
                                // Cumulative thread-CPU time for operation.
    uint64_t        num_ops;    // # of msg-op requests received
    req_resp_type_t last_mtype; // Last requests->mtype
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
    printf("Server clocks resolution"
           ": CLOCK_REALTIME=%" PRIu64 " ns"
           ", CLOCK_MONOTONIC=%" PRIu64 " ns"
           ", CLOCK_PROCESS_CPUTIME_ID=%" PRIu64 " ns"
           ", CLOCK_THREAD_CPUTIME_ID=%" PRIu64 " ns"
           "\n",
           timespec_to_ns(&ts_real),
           timespec_to_ns(&ts_monotonic),
           timespec_to_ns(&ts_processCPU),
           timespec_to_ns(&ts_threadCPU));
}

int
main(int argc, char *argv[])
{
    ssize_t msgLen;
    int     serverId;
    struct sigaction sa;

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
    const char *logfile = "/tmp/l3.c-server-test.dat";
    int e = l3_init(logfile);
    if (e) {
        errExit("l3_init");
    }

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

    printf("Server: Initiate L3-logging to log-file '%s'"
           ", using %s encoding scheme.\n", logfile, loc_scheme);

#endif // L3_ENABLED

    /* Read requests, handle each in a separate child process */
    requestMsg req;
    responseMsg resp;

    // On MacOSX, only the CLOCK_THREAD_CPUTIME_ID clock has a resolution of 1ns.
    // On Linux, all clocks seem to have 1 ns resolution, so use real-time clock.
    int one_ns_res_clock_id;
#if __APPLE__
    one_ns_res_clock_id = CLOCK_THREAD_CPUTIME_ID;
#else
    one_ns_res_clock_id = CLOCK_REALTIME;
#endif  // __APPLE__

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
            printf("Server: Client ID=%d joined. # active clients=%d (HWM=%d)\n",
                   req.clientId, NumActiveClients, NumActiveClientsHWM);
            break;

          case REQ_MT_INCR:
            if (clock_gettime(one_ns_res_clock_id, &ts0)) {     // Timing begins
                errExit("clock_gettime-ts0");
            }
            clientp = &ActiveClients[req.client_idx];
            assert(clientp->clientId == req.clientId);

            // Construct response message
            resp.mtype = REQ_MT_INCR;
            resp.clientId = req.clientId;   // For this client
            resp.client_idx = req.client_idx;

            // Implement and record increment.
            clientp->last_mtype = REQ_MT_INCR;
            resp.counter = ++clientp->client_ctr;
            clientp->num_ops++;

            if (clock_gettime(one_ns_res_clock_id, &ts1)) {     // Timing ends
                errExit("clock_gettime-ts1");
            }

            nsec0 = timespec_to_ns(&ts0);
            nsec1 = timespec_to_ns(&ts1);
            elapsed_ns = (nsec1 - nsec0);
            clientp->cumu_thread_cpu_time_ns += elapsed_ns;

#if L3_ENABLED
            // Record time-consumed, clarifying semantic of elapsed time.
            l3_log_simple("Server msg: Increment: ClientID=%d, "

#if __APPLE__
                          "Thread-CPU-"
#else
                          "Elapsed real-"
#endif  // __APPLE__
                          "time=%" PRIu64 " ns.", resp.clientId, elapsed_ns);
#endif // L3_ENABLED
            break;

          case REQ_MT_EXIT:

            clientp = &ActiveClients[req.client_idx];
            // One client has informed that it has exited
            NumActiveClients--;

#if __APPLE__
            const char *time_msg = "Thread-CPU-time";
#else
            const char *time_msg = "Elapsed real-time";
#endif  // __APPLE__
            printf("Server: Client ID=%d exited. num_ops=%" PRIu64
                   ", Avg. %s=%" PRIu64 " ns/msg, # active clients=%d\n",
                   req.clientId,
                   clientp->num_ops,
                   time_msg,
                   (clientp->cumu_thread_cpu_time_ns / clientp->num_ops),
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
    exit(EXIT_SUCCESS);
}
