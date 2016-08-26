#include <stdbool.h>
#include "serverconnection.h"
#include "fmconfig.h"
#include "fmlog.h"
#include "cmdline.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


static void mainloop(void)
{
    int i, listenfd, acceptfd;
    unsigned connCount = 0, idleConnCount, busyConnCount, maxConnCount;
    struct sockaddr_in addr;
    ServerConnection **connections, **busyConnections;
    DataReadySelector *drs;
    bool isConnMaxWarnPrinted = false;

    maxConnCount = config_getMaxClients();
    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
        log_fatal("socket");
    i = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_getListenPort());
    if( bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0 )
        log_fatal("bind");
    if( listen(listenfd, maxConnCount) < 0 )
        log_fatal("listen");
    if( ! config_switchToTargetUser() )
        exit(1);
    connections = malloc(maxConnCount * sizeof(ServerConnection*));
    busyConnections = malloc(maxConnCount * sizeof(ServerConnection*));
    drs_setNonBlockingCloExecFlags(listenfd);
    drs = drs_new();
    while( 1 ) {
        if( connCount < maxConnCount )
            drs_setReadFd(drs, listenfd);
        else if( ! isConnMaxWarnPrinted ) {
            log_warn("number of clients reached maximum (%u)", maxConnCount);
            isConnMaxWarnPrinted = true;
        }
        drs_select(drs);
        while( connCount < maxConnCount &&
                (acceptfd = accept(listenfd, NULL, NULL)) >= 0 )
        {
            i = 1;
            setsockopt(acceptfd, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(i));
            drs_setNonBlockingCloExecFlags(acceptfd);
            connections[connCount++] = conn_new(acceptfd);
        }
        if( connCount > maxConnCount && errno != EWOULDBLOCK )
            log_fatal("accept");
        /* keep connections ordered descending by idle time;
         * close most idle connection when connection number reached maximum
         */
        i = 0;
        idleConnCount = busyConnCount = 0;
        while( i < connCount ) {
            switch( conn_processDataReady(connections[i], drs,
                    connCount == maxConnCount && i == busyConnCount) )
            {
            case CONN_BUSY:
                busyConnections[busyConnCount++] = connections[i++];
                break;
            case CONN_IDLE:
                if( i != idleConnCount )
                    connections[idleConnCount] = connections[i];
                ++idleConnCount;
                ++i;
                break;
            default: /* CONN_TO_CLOSE */
                conn_free(connections[i]);
                ++i;
                break;
            }
        }
        for(i = 0; i < busyConnCount; ++i)
            connections[idleConnCount+i] = busyConnections[i];
        connCount = idleConnCount + busyConnCount;
    }
}

static void mainloop_inetd(void)
{
    ServerConnection *connection = NULL;
    DataReadySelector *drs;

    if( ! config_switchToTargetUser() )
        exit(1);
    drs_setNonBlockingCloExecFlags(0);
    drs = drs_new();
    connection = conn_new(0);
    while( conn_processDataReady(connection, drs, false) != CONN_TO_CLOSE )
        drs_select(drs);
    conn_free(connection);
    drs_free(drs);
}

int main(int argc, char *argv[])
{
    if( cmdline_parse(argc, argv) ) {
        signal(SIGPIPE, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);
        config_parse();
        if( cmdline_isInetdMode() )
            mainloop_inetd();
        else
            mainloop();
    }
    return 0;
}

