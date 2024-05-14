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
 * svmsg_file_client.c
 * ****************************************************************************

   The server and client communicate using System V message queues.

   This version of the application has been extensively modified based
   on an initial version downloaded from:
   https://man7.org/tlpi/code/online/all_files_by_chapter.html

   Multiple clients can connect to a single server, which is receiving
   messages on a specific server-queue-ID. Server will respond to each client
   separately on its established client-queue-ID. [See svmsg_file_server.c]

   Clients operation is simple:

   - Send a message to the server containing an int64_t number
   - Receive the number incremented / decremented by the server.
   - Keep looping on this exchange for specified number of iterations.

   The idea is to just have a fast message-exchange between the client
   and the server and to measure the round-trip time. The actual work
   done on the server-side is very little; just a number++ / number--.

   We, then, inject L3-logging on the server-side, to record the
   exchange with the client. The idea is to run this experiment with
   L3-logging off (baseline) and compare average elapsed-time metrics
   of round-trip of one message using a build where L3-logging code is
   activated on the server side.
 * ****************************************************************************
*/
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>

#include "svmsg_file.h"

// L3-LOC related headers
#include "l3.h"
#include "size_str.h"

// Useful constants
#define L3_100K         ((uint32_t) (100 * 1000))

static int clientId;

static void
removeQueue(void)
{
    if (msgctl(clientId, IPC_RMID, NULL) == -1) {
        errExit("msgctl");
    }
}

/**
 * ****************************************************************************
 * main() - Client program.
 * ****************************************************************************
 */
int
main(int argc, char *argv[])
{
    ssize_t msgLen;
    size_t niters = 100;

#if __APPLE__
    printf("%s is currently not supported on Mac/OSX\n", argv[0]);
    exit(EXIT_SUCCESS);
#endif

    if (((argc == 2) && strcmp(argv[1], "--help") == 0) || (argc > 2)) {
        printf("%s [ <number-of-iterations> ]\n"
               "Default: %lu iterations.\n",
               argv[0], niters);
        exit(EXIT_SUCCESS);
    }

    /* Get server's queue identifier; create queue for response */

    int serverId = msgget(SERVER_KEY, S_IWUSR);
    if (serverId == -1) {
        errExit("msgget - server message queue");
    }

    clientId = msgget(IPC_PRIVATE, (S_IRUSR | S_IWUSR | S_IWGRP));
    if (clientId == -1) {
        errExit("msgget - client message queue");
    }

    if (atexit(removeQueue) != 0) {
        errExit("atexit");
    }

    if (argc == 2) {
        niters = atoi(argv[1]);
    }

    printf("Client ID=%d Perform %lu (%s) message-exchanges to"
           " increment a number"
#if L3_ENABLED
           ", with L3-logging"

// In build -D L3_LOC_ELF_ENABLED and -D L3_LOC_ENABLED are both ON.
// So, check in this order.
#if L3_LOC_ELF_ENABLED
           ", LOC-ELF encoding,"
#elif L3_LOC_ENABLED
           ", default LOC encoding,"
#endif
           " enabled on server-side"

#endif // L3_ENABLED
           ".\n", clientId, niters, value_str(niters));

    requestMsg req;
    req.mtype       = REQ_MT_INIT;     // New client is establishing a connection
    req.clientId    = clientId;
    req.client_idx  = REQ_CLIENT_INDEX_UNKNOWN;
    req.counter     = 0;

    if (msgsnd(serverId, &req, REQ_MSG_SIZE, 0) == -1) {
        errExit("msgsnd");
    }

    responseMsg resp;
    msgLen = msgrcv(clientId, &resp, RESP_MSG_SIZE, 0, 0);
    if (msgLen == -1) {
        errExit("msgrcv");
    }

    if (resp.mtype == RESP_MT_FAILURE) {
        /* Display msg from server */
        printf("Counter=%" PRIu64 "\n", resp.counter);
        exit(EXIT_FAILURE);
    }

    // Re-establish client's identity as obtained from the server.
    req.client_idx = resp.client_idx;

    /* Loop around for requested # of iterations, or till the time the
     * server informs us to quit. (We implement receiving REQ_MT_QUIT from
     * the server, but that is not happening right now.)
     */
    size_t ictr;

    // Msg-round trip timing measurements for each msgsend/recv() pair.
    struct timespec ts0;
    struct timespec ts1;
    uint64_t        elapsed_ns = 0;

    req.mtype = REQ_MT_INCR;    // All ops are simply an increment

    if (clock_gettime(CLOCK_REALTIME, &ts0)) {          // **** Timing begins
        errExit("clock_gettime-ts0");
    }
    for (ictr = 0; ictr < niters; ictr++) {                 // Perform n-iterations

        // DEBUG: if ((ictr % L3_100K) == 0) { printf("."); fflush(stdout); }

        req.counter = resp.counter;

        if (msgsnd(serverId, &req, REQ_MSG_SIZE, 0) == -1) {    // --> Send
            errExit("msgsnd");
        }

        msgLen = msgrcv(clientId, &resp, RESP_MSG_SIZE, 0, 0);  // <-- Recv
        if (msgLen == -1) {
            errExit("msgrcv");
        }
        if (resp.mtype == RESP_MT_FAILURE) {
            /* Display msg from server */
            printf("Counter=%" PRIu64 "\n", resp.counter);
            exit(EXIT_FAILURE);
        }

        // Early-quit msg received from server
        if (resp.mtype == REQ_MT_QUIT) {
            break;
        }
    }
    if (clock_gettime(CLOCK_REALTIME, &ts1)) {          // **** Timing ends
        errExit("clock_gettime-ts1");
    }
    uint64_t nsec0 = timespec_to_ns(&ts0);
    uint64_t nsec1 = timespec_to_ns(&ts1);
    elapsed_ns += (nsec1 - nsec0);                      // Elapsed-ns

    size_t throughput = (uint64_t)
                ( ((ictr * 1.0) / elapsed_ns) * L3_NS_IN_SEC );

    req.mtype = REQ_MT_SET_THROUGHPUT;
    req.counter = throughput;
    // Send our throughput, so server can aggregate across all clients.
    if (msgsnd(serverId, &req, REQ_MSG_SIZE, 0) == -1) {
        errExit("msgsnd-throughput");
    }

    req.mtype = REQ_MT_EXIT;
    if (msgsnd(serverId, &req, REQ_MSG_SIZE, 0) == -1) {
        errExit("msgsnd-exit");
    }
    printf("Client: ID=%d Performed %lu (%s) message send/receive operations"
           ", ctr=%" PRIu64 ", Avg. %" PRIu64 " ns/msg"
           ", Client-throughput=%lu (%s) ops/sec."
           " Exiting.\n",
            clientId, ictr, value_str(ictr), resp.counter,
            (elapsed_ns / ictr),
            throughput, value_str(throughput));

    exit(EXIT_SUCCESS);
}
