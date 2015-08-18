#include <stdbool.h>
#include "serverconnection.h"
#include "reqhandler.h"
#include "membuf.h"
#include "auth.h"
#include "fmlog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>


enum RequestReadState {
    RRS_READ_HEAD,
    RRS_READ_BODY,
    RRS_READ_TRAILER,
    RRS_READ_FINISHED
};

struct ServerConnection {
    int socketFd;
    char readBuffer[65536];
    unsigned readOffset;
    unsigned readSize;
    enum RequestReadState rrs;
    RequestHeader *header;
    MemBuf *chunkHdr;
    RequestHandler *handler;
    unsigned long long bodyLen;
    unsigned long long bodyReadLen;
};

ServerConnection *conn_new(int socketFd)
{
    ServerConnection *conn = malloc(sizeof(ServerConnection));

    conn->socketFd = socketFd;
    conn->readOffset = 0;
    conn->readSize = 0;
    conn->rrs = RRS_READ_HEAD;
    conn->header = reqhdr_new();
    conn->chunkHdr = NULL;
    conn->handler = NULL;
    conn->bodyLen = 0;
    conn->bodyReadLen = 0;
    return conn;
}

static void onFinishedHeader(ServerConnection *conn)
{
    const char *val = NULL;
    struct sockaddr_in peer;
    socklen_t peerAddrLen;
    char peerAddrStr[INET_ADDRSTRLEN];

    peerAddrLen = sizeof(peer);
    if(getpeername(conn->socketFd, (struct sockaddr*)&peer, &peerAddrLen) == 0)
        val = inet_ntop(AF_INET, &peer.sin_addr, peerAddrStr, peerAddrLen);
    conn->handler = reqhdlr_new(conn->header, val);
    if( reqhdr_isChunkedTransferEncoding(conn->header) ) {
        conn->chunkHdr = mb_newWithStr("\r\n");
        conn->rrs = RRS_READ_BODY;
    }else{
        if( (val = reqhdr_getHeaderVal(conn->header, "Content-Length")) != NULL)
            conn->bodyLen = strtoull(val, NULL, 10);
        if( conn->bodyLen > 0 ) {
            conn->rrs = RRS_READ_BODY;
        }else
            conn->rrs = RRS_READ_FINISHED;
    }
}

static void appendBodyData(ServerConnection *conn, DataReadySelector *drs)
{
    const char *bol, *eol, *const dataEnd = conn->readBuffer + conn->readSize;
    unsigned addLen, processed;

    bol = conn->readBuffer + conn->readOffset;
    while( conn->rrs == RRS_READ_BODY && bol < dataEnd ) {
        if( conn->bodyReadLen < conn->bodyLen ) {
            addLen = dataEnd - bol;
            if( conn->bodyReadLen + addLen > conn->bodyLen )
                addLen = conn->bodyLen - conn->bodyReadLen;
            processed = reqhdlr_processData(conn->handler, bol, addLen, drs);
            conn->bodyReadLen += processed;
            bol += processed;
            if( processed < addLen )
                break;
        }
        if( conn->bodyReadLen == conn->bodyLen ) {
            if( conn->chunkHdr != NULL ) {
                if( mb_dataLen(conn->chunkHdr) < 2 ) {
                    addLen = dataEnd - bol < 2 - mb_dataLen(conn->chunkHdr) ?
                        dataEnd - bol : 2 - mb_dataLen(conn->chunkHdr);
                    mb_appendData(conn->chunkHdr, bol, addLen);
                    bol += addLen;
                }
                eol = memchr(bol, '\n', dataEnd - bol);
                addLen = (eol==NULL ? dataEnd : eol) - bol;
                mb_appendData(conn->chunkHdr, bol, addLen);
                if( eol != NULL ) {
                    addLen = strtoul(mb_data(conn->chunkHdr)+2, NULL, 16);
                    if( addLen == 0 ) { /* end of body */
                        conn->rrs = RRS_READ_TRAILER;
                    }else{
                        conn->bodyLen += addLen;
                    }
                    bol = eol + 1;
                    mb_resize(conn->chunkHdr, 0);
                }else{
                    bol = dataEnd;
                }
            }else{
                conn->rrs = RRS_READ_FINISHED;
            }
        }
    }
    if( bol == dataEnd )
        conn->readOffset = conn->readSize = 0;
    else
        conn->readOffset = bol - conn->readBuffer;
}

static void appendData(ServerConnection *conn, DataReadySelector *drs)
{
    enum RequestReadState rrsSav = conn->rrs;

    if( conn->rrs == RRS_READ_HEAD ) {
        int offset = reqhdr_appendData(conn->header,
                conn->readBuffer + conn->readOffset,
                conn->readSize - conn->readOffset);
        if( offset >= 0 ) {
            conn->readOffset += offset;
            if( conn->readOffset == conn->readSize )
                conn->readOffset = conn->readSize = 0;
            onFinishedHeader(conn);
        }else{
            conn->readOffset = conn->readSize = 0;
        }
    }
    if( conn->rrs == RRS_READ_BODY && conn->readSize ) {
        appendBodyData(conn, drs);
    }
    if( conn->rrs == RRS_READ_TRAILER && conn->readSize ) {
        int offset = reqhdr_appendData(conn->header,
                conn->readBuffer + conn->readOffset,
                conn->readSize - conn->readOffset);
        if( offset >= 0 ) {
            conn->rrs = RRS_READ_FINISHED;
            conn->readOffset += offset;
            if( conn->readOffset == conn->readSize )
                conn->readOffset = conn->readSize = 0;
        }else
            conn->readOffset = conn->readSize = 0;
    }
    if( rrsSav != RRS_READ_FINISHED && conn->rrs == RRS_READ_FINISHED)
        reqhdlr_requestReadCompleted(conn->handler, conn->header);
}

bool conn_processDataReady(ServerConnection *conn, DataReadySelector *drs)
{
    int rd;
    bool freeConn = false;

    while( true ) {
        if( conn->readSize ) {
            appendData(conn, drs);
        }
        if( conn->readSize != 0 )
            break;
        if( (rd = read(conn->socketFd, conn->readBuffer,
                sizeof(conn->readBuffer))) <= 0 )
        {
            if( rd < 0 ) {
                if( errno != EWOULDBLOCK ) {
                    if( errno != ECONNRESET )
                        log_error("read");
                    freeConn = true;
                }
            }else if( rd == 0 ) /* premature EOF */
                freeConn = true;
            break;
        }
        conn->readSize = rd;
    }
    if( conn->handler != NULL )
        freeConn = reqhdlr_progressResponse(conn->handler, conn->socketFd, drs);
    if( ! freeConn && conn->rrs != RRS_READ_FINISHED && conn->readSize == 0 )
        drs_setReadFd(drs, conn->socketFd);
    return freeConn;
}

void conn_free(ServerConnection *conn)
{
    if( conn != NULL ) {
        close(conn->socketFd);
        reqhdr_free(conn->header);
        mb_free(conn->chunkHdr);
        reqhdlr_free(conn->handler);
        free(conn);
    }
}

