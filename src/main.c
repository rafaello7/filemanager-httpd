#include <stdbool.h>
#include "reqhandler.h"
#include "fmconfig.h"
#include "fmlog.h"
#include "cmdline.h"
#include "auth.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


struct ServerConnection {
    int fd;
    RequestBuf *request;
    DataSource *response;
};

static void freeConn(struct ServerConnection *conn)
{
    close(conn->fd);
    conn->fd = -1;
    req_free(conn->request);
    ds_free(conn->response);
}

static void mainloop(void)
{
    int i, listenfd, acceptfd, fdMax, connCount = 0, rd, wr, fdFlags, sysErrNo;
    struct sockaddr_in addr;
    struct ServerConnection *connections = NULL, *conn;
    fd_set readFds, writeFds;
    bool isSuccess;
    char buf[65536];

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
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);

    while( 1 ) {
        FD_SET(listenfd, &readFds);
        fdMax = listenfd;
        for(i = 0; i < connCount; ++i) {
            conn = connections + i;
            if( conn->response == NULL ) {
                FD_SET(conn->fd, &readFds);
            }else{
                FD_SET(conn->fd, &writeFds);
            }
            if( conn->fd > fdMax )
                fdMax = conn->fd;
        }
        if( select(fdMax+1, &readFds, &writeFds, NULL, NULL) < 0 )
            log_fatal("select");
        if( FD_ISSET(listenfd, &readFds) ) {
            if( (acceptfd = accept(listenfd, NULL, NULL)) < 0 )
                log_fatal("accept");
            if( (fdFlags = fcntl(acceptfd, F_GETFL)) == -1 )
                log_fatal("fcntl(F_GETFL)");
            if( fcntl(acceptfd, F_SETFL, fdFlags | O_NONBLOCK) < 0 )
                log_fatal("fcntl(F_SETFL)");
            connections = realloc(connections,
                    (connCount+1) * sizeof(struct ServerConnection));
            conn = connections + connCount;
            conn->fd = acceptfd;
            conn->request = req_new();
            conn->response = NULL;
            ++connCount;
        }
        i = 0;
        while( i < connCount ) {
            conn = connections + i;
            if( FD_ISSET(conn->fd, &readFds) ) {
                while( (rd = read(conn->fd, buf, sizeof(buf)-1)) > 0 &&
                        (wr = req_appendData(conn->request, buf, rd)) < 0 )
                    ;
                if( rd < 0 ) {
                    if( errno != EWOULDBLOCK )
                        log_fatal("read");
                }else if( rd == 0 ) {
                    /* premature EOF */
                    FD_CLR(conn->fd, &readFds);
                    freeConn(conn);
                }else{  /* rd > 0  =>  wr >= 0 */
                    conn->response = reqhdlr_processRequest(conn->request);
                    req_free(conn->request);
                    conn->request = NULL;
                }
            }else if( FD_ISSET(conn->fd, &writeFds) ) {
                if( (isSuccess = ds_write(conn->response, conn->fd, &sysErrNo))
                        || sysErrNo != EWOULDBLOCK )
                {
                    /* ECONNRESET occurs when peer has closed connection
                     * without receiving all data; similar EPIPE.
                     * Both not worthy to notify */
                    if( !isSuccess && errno != ECONNRESET && errno != EPIPE )
                        log_error("connected socket write failed");
                    FD_CLR(conn->fd, &writeFds);
                    freeConn(conn);
                }
            }
            if( conn->fd == -1 ) {
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

