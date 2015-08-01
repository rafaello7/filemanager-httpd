#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include "filemanager.h"
#include "fmconfig.h"
#include "cmdline.h"

static void fatal(const char *msg, ...)
{
    int err = errno;
    va_list args;

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    if( err != 0 )
        fprintf(stderr, ": %s", strerror(err));
    fprintf(stderr, "\n");
    exit(1);
}

struct ServerConnection {
    int fd;
    RequestBuf *request;
    MemBuf *response;
    int responseOff;
};

static void freeConn(struct ServerConnection *conn)
{
    close(conn->fd);
    conn->fd = -1;
    req_free(conn->request);
    mb_free(conn->response);
}

static void prepareResponse(struct ServerConnection *conn)
{
    RespBuf *resp = NULL;
    const char *meth = req_getMethod(conn->request);
    int isHeadReq = ! strcmp(meth, "HEAD");

    if( strcmp(meth, "GET") && strcmp(meth, "POST") && ! isHeadReq ) {
        resp = resp_new(HTTP_405_METHOD_NOT_ALLOWED);
        resp_appendHeader(resp, "Allow", "GET, HEAD, POST");
    }else{
        resp = filemgr_processRequest(conn->request);
    }
    resp_appendHeader(resp, "Connection", "close");
    resp_appendHeader(resp, "Server", "filemanager-httpd");
    conn->response = resp_finish(resp, isHeadReq);
    conn->responseOff = 0;
}

static void mainloop(void)
{
    int i, listenfd, acceptfd, fdMax, connCount = 0, rd, wr;
    struct sockaddr_in addr;
    struct ServerConnection *connections = NULL, *conn;
    fd_set readFds, writeFds;
    char buf[65536];

    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
        fatal("socket");
    i = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_getListenPort());
    if( bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0 )
        fatal("bind");
    if( listen(listenfd, 5) < 0 )
        fatal("listen");
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
            fatal("select");
        if( FD_ISSET(listenfd, &readFds) ) {
            if( (acceptfd = accept(listenfd, NULL, NULL)) < 0 )
                fatal("accept");
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
                if( (rd = read(conn->fd, buf, sizeof(buf)-1)) < 0 )
                    fatal("read");
                if( rd != 0 ) {
                    wr = req_appendData(conn->request, buf, rd);
                    if( wr >= 0 ) {
                        prepareResponse(conn);
                    }
                }else{
                    FD_CLR(conn->fd, &readFds);
                    freeConn(conn);
                }
            }else if( FD_ISSET(conn->fd, &writeFds) ) {
                if( (wr = write(conn->fd,
                        mb_data(conn->response) + conn->responseOff,
                        mb_dataLen(conn->response) - conn->responseOff)) < 0 )
                {
                    /* ECONNRESET occurs when peer has closed connection
                     * without receiving all data; not worthy to notify */
                    if( errno != ECONNRESET )
                        perror("write");
                    conn->responseOff = mb_dataLen(conn->response);
                }else{
                    conn->responseOff += wr;
                }
                if( conn->responseOff == mb_dataLen(conn->response) ) {
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
        config_parse();
        mainloop();
    }
    return 0;
}

