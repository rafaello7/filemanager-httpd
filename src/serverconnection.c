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

    log_debug("================================= %d open", socketFd);
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

static void appendBodyData(ServerConnection *conn, DataProcessingResult *dpr)
{
    const char *bol, *eol, *const dataEnd = conn->readBuffer + conn->readSize;
    unsigned addLen, processed;

    bol = conn->readBuffer + conn->readOffset;
    while( conn->rrs == RRS_READ_BODY && bol < dataEnd ) {
        if( conn->bodyReadLen < conn->bodyLen ) {
            addLen = dataEnd - bol;
            if( conn->bodyReadLen + addLen > conn->bodyLen )
                addLen = conn->bodyLen - conn->bodyReadLen;
            processed = reqhdlr_processData(conn->handler, bol, addLen, dpr);
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

static void appendData(ServerConnection *conn, DataProcessingResult *dpr)
{
    enum RequestReadState rrsSav = conn->rrs;

    if( conn->rrs == RRS_READ_HEAD ) {
        int offset = reqhdr_appendData(conn->header,
                conn->readBuffer + conn->readOffset,
                conn->readSize - conn->readOffset);
        if( offset >= 0 ) {
            log_debug("%d request: %s %s HTTP/%s", conn->socketFd,
                    reqhdr_getMethod(conn->header),
                    reqhdr_getPath(conn->header),
                    reqhdr_getVersion(conn->header));
            if( log_isLevel(2) ) {
                unsigned i = 0;
                const char *name, *value;

                while( reqhdr_getHeaderAt(conn->header, i, &name, &value) ) {
                    log_debug("%s: %s", name, value);
                    ++i;
                }
            }
            conn->readOffset += offset;
            if( conn->readOffset == conn->readSize )
                conn->readOffset = conn->readSize = 0;
            onFinishedHeader(conn);
        }else{
            conn->readOffset = conn->readSize = 0;
        }
    }
    if( conn->rrs == RRS_READ_BODY && conn->readSize ) {
        appendBodyData(conn, dpr);
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
    const char *hdrVal;
    DataProcessingResult dpr;

    dpr_init(&dpr);
    while( true ) {
        while( ! dpr.closeConn && dpr.reqState == DPR_READY
                && conn->rrs != RRS_READ_FINISHED )
        { 
            if( conn->readSize == 0 ) {
                if( (rd = read(conn->socketFd, conn->readBuffer,
                        sizeof(conn->readBuffer))) > 0 )
                {
                    conn->readSize = rd;
                }else if( rd < 0 ) {
                    if( errno == EWOULDBLOCK ) {
                        dpr_setReqState(&dpr, DPR_AWAIT_READ, conn->socketFd);
                    }else{
                        if( errno != ECONNRESET )
                            log_error("read");
                        dpr_setCloseConn(&dpr);
                    }
                }else if( rd == 0 ) {/* EOF */
                    dpr_setCloseConn(&dpr);
                }
            }
            if( conn->readSize != 0 )
                appendData(conn, &dpr);
        }
        if( ! dpr.closeConn && conn->handler != NULL ) {
            if(reqhdlr_progressResponse(conn->handler, conn->socketFd, &dpr)) {
                /* response send has been finished */
                reqhdlr_free(conn->handler);
                conn->handler = NULL;
            }
        }
        if( conn->rrs != RRS_READ_FINISHED || conn->handler != NULL )
            break;
        /* request processing has been completed */
        /* sanity check: no await data, socket is ready */
        if( dpr.closeConn || dpr.reqState != DPR_READY ||
                dpr.respState != DPR_READY )
            log_fatal("INTERNAL ERROR: conn_processDataReady closeConn=%d, "
                    "reqState=%d, respState=%d", dpr.closeConn, dpr.reqState,
                    dpr.respState);
        /* return if HTTP/1.0 or request has "Connection: close" */
        if( ! strcmp(reqhdr_getVersion(conn->header), "1.0") ||
            ((hdrVal = reqhdr_getHeaderVal(conn->header, "Connection"))
                != NULL && !strcmp(hdrVal, "close")) )
        {
            dpr_setCloseConn(&dpr);
            break;
        }
        /* re-intialize */
        conn->rrs = RRS_READ_HEAD;
        reqhdr_free(conn->header);
        conn->header = reqhdr_new();
        mb_free(conn->chunkHdr);
        conn->chunkHdr = NULL;
        conn->bodyLen = 0;
        conn->bodyReadLen = 0;
    }
    if( ! dpr.closeConn ) {
        if( dpr.reqState == DPR_AWAIT_READ )
            drs_setReadFd(drs, dpr.reqAwaitFd);
        else if( dpr.reqState == DPR_AWAIT_WRITE )
            drs_setWriteFd(drs, dpr.reqAwaitFd);
        if( dpr.respState == DPR_AWAIT_READ )
            drs_setReadFd(drs, dpr.respAwaitFd);
        else if( dpr.respState == DPR_AWAIT_WRITE )
            drs_setWriteFd(drs, dpr.respAwaitFd);
    }
    return dpr.closeConn;
}

void conn_free(ServerConnection *conn)
{
    if( conn != NULL ) {
        log_debug("================================ %d close", conn->socketFd);
        close(conn->socketFd);
        reqhdr_free(conn->header);
        mb_free(conn->chunkHdr);
        reqhdlr_free(conn->handler);
        free(conn);
    }
}

