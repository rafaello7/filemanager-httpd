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
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


static void mainloop(void)
{
    int i, listenfd, acceptfd, connCount = 0;
    struct sockaddr_in addr;
    ServerConnection **connections = NULL;
    DataReadySelector *drs;

    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
        log_fatal("socket");
    i = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_getListenPort());
    if( bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0 )
        log_fatal("bind");
    if( listen(listenfd, 5) < 0 )
        log_fatal("listen");
    if( ! config_switchToTargetUser() )
        exit(1);
    drs_setNonBlockingCloExecFlags(listenfd);
    drs = drs_new();
    while( 1 ) {
        drs_setReadFd(drs, listenfd);
        drs_select(drs);
        while( (acceptfd = accept(listenfd, NULL, NULL)) >= 0 ) {
            drs_setNonBlockingCloExecFlags(acceptfd);
            connections = realloc(connections,
                    (connCount+1) * sizeof(ServerConnection*));
            connections[connCount] = conn_new(acceptfd);
            ++connCount;
        }
        if( errno != EWOULDBLOCK )
            log_fatal("accept");
        i = 0;
        while( i < connCount ) {
            if( conn_processDataReady(connections[i], drs) ) {
                conn_free(connections[i]);
                if( i < connCount - 1 )
                    connections[i] = connections[connCount-1];
                --connCount;
            }else
                ++i;
        }
    }

}

int main(int argc, char *argv[])
{
    if( cmdline_parse(argc, argv) ) {
        signal(SIGPIPE, SIG_IGN);
        config_parse();
        mainloop();
    }
    return 0;
}

