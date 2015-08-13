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
    char *chunkHdr;
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
    int isChunked = 0;
    const char *val;

    if( (val = reqhdr_getHeaderVal(conn->header, "Transfer-Encoding")) != NULL) {
        while( ! isChunked && val ) {
            val += strspn(val, ", \t");
            isChunked = !strncasecmp(val, "chunked", 7);
            val = strchr(val, ',');
        }
    }
    conn->handler = reqhdlr_new(conn->header);
    if( isChunked ) {
        conn->chunkHdr = strdup("");
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
    unsigned curLen, addLen, processed;

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
                curLen = strlen(conn->chunkHdr);
                eol = memchr(bol, '\n', dataEnd - bol);
                addLen = (eol==NULL ? dataEnd : eol) - bol;
                conn->chunkHdr = realloc(conn->chunkHdr,
                        curLen + addLen + 1);
                memcpy(conn->chunkHdr + curLen, bol, addLen);
                curLen += addLen;
                conn->chunkHdr[curLen] = '\0';
                if( eol != NULL ) {
                    addLen = strtoul(conn->chunkHdr, NULL, 16);
                    if( addLen == 0 ) {
                        conn->bodyLen -= 2;      /* strip CRLF */
                        conn->rrs = RRS_READ_TRAILER;
                    }else if( conn->bodyLen == 0 ) {
                        conn->bodyLen = addLen+2;
                    }else{
                        conn->bodyLen += addLen;
                        conn->bodyReadLen -= 2; /* strip CRLF*/
                    }
                    bol = eol + 1;
                    conn->chunkHdr[0] = '\0';
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
    int rd, sysErrNo;
    bool isSuccess, freeConn;

    if( drs_clearReadFd(drs, conn->socketFd) || conn->readSize ) {
        while( true ) {
            if( conn->readSize ) {
                appendData(conn, drs);
                if( conn->readSize != 0 )
                    break;
            }
            if( conn->readSize == 0 ) {
                if( (rd = read(conn->socketFd, conn->readBuffer,
                        sizeof(conn->readBuffer))) <= 0 )
                {
                    if( rd < 0 ) {
                        if( errno != EWOULDBLOCK )
                            log_fatal("read");
                    }else if( rd == 0 ) /* premature EOF */
                        freeConn = true;
                    break;
                }
                conn->readSize = rd;
            }
        }
    }
    if( drs_clearWriteFd(drs, conn->socketFd) ) {
        if( (isSuccess = reqhdlr_emitResponseBytes(conn->handler,
                        conn->socketFd, &sysErrNo))
                || sysErrNo != EWOULDBLOCK )
        {
            /* ECONNRESET occurs when peer has closed connection
             * without receiving all data; similar EPIPE.
             * Both not worthy to notify */
            if( !isSuccess && sysErrNo != ECONNRESET && sysErrNo != EPIPE )
                log_error("connected socket write failed");
            freeConn = true;
        }
    }
    if( ! freeConn ) {
        if( conn->rrs != RRS_READ_FINISHED && conn->readSize == 0 )
            drs_setReadFd(drs, conn->socketFd);
        if( conn->rrs == RRS_READ_FINISHED )
            drs_setWriteFd(drs, conn->socketFd);
    }
    return freeConn;
}

void conn_free(ServerConnection *conn)
{
    if( conn != NULL ) {
        close(conn->socketFd);
        reqhdr_free(conn->header);
        free(conn->chunkHdr);
        reqhdlr_free(conn->handler);
        free(conn);
    }
}

