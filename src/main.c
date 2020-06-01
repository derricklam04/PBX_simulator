#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "csapp.h"
#include "pbx.h"
#include "server.h"
#include "debug.h"

static void terminate(int status);

static void SIGHUP_handler(int sig){
    if (sig == SIGHUP){
        debug("SIGHUP received in main.c");
        terminate(EXIT_SUCCESS);
    }
}

/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Perform required initialization of the PBX module.
    debug("Initializing PBX...");
    debug("%d", getpid());
    pbx = pbx_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    if (argc != 3 || strcmp(argv[1], "-p") != 0 ){
        fprintf(stderr, "[Error] Usage: %s -p <port>\n", argv[0]);
        exit(1);
    }

    struct sigaction act;
    memset(&act, 0 , sizeof(act));
    act.sa_handler = SIGHUP_handler;
    sigaction(SIGHUP, &act, NULL);

    socklen_t clientlen;
    int listenfd;
    int *connfdptr;
    pthread_t tid;
    struct sockaddr_storage clientaddr;

    listenfd = Open_listenfd(argv[2]);

    while (1) {
        clientlen=sizeof(struct sockaddr_storage);
        connfdptr = Malloc(sizeof(int));
        *connfdptr = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, pbx_client_service, connfdptr);
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the PBX server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    debug("Shutting down PBX...");
    pbx_shutdown(pbx);
    debug("PBX server terminating");
    exit(status);
}
