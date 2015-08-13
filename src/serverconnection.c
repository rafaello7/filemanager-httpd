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

static int appendBodyData(ServerConnection *conn, const char *data,
        unsigned len)
{
    const char *bol, *eol;
    unsigned curLen, addLen;

    bol = data;
    while( conn->rrs == RRS_READ_BODY && bol < data + len ) {
        if( conn->bodyReadLen < conn->bodyLen ) {
            addLen = data + len - bol;
            if( conn->bodyReadLen + addLen > conn->bodyLen )
                addLen = conn->bodyLen - conn->bodyReadLen;
            reqhdlr_consumeBodyBytes(conn->handler, bol, addLen);
            conn->bodyReadLen += addLen;
            bol += addLen;
        }
        if( conn->bodyReadLen == conn->bodyLen ) {
            if( conn->chunkHdr != NULL ) {
                curLen = strlen(conn->chunkHdr);
                eol = memchr(bol, '\n', len - (bol-data));
                addLen = (eol==NULL ? data+len : eol) - bol;
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
                    bol = data + len;
                }
            }else{
                conn->rrs = RRS_READ_FINISHED;
            }
        }
    }
    return data+len - bol;
}

static int appendData(ServerConnection *conn, const char *data, unsigned len)
{
    int offset = 0;
    enum RequestReadState rrsSav = conn->rrs;

    if( conn->rrs == RRS_READ_HEAD ) {
        offset = reqhdr_appendData(conn->header, data, len);
        if( offset >= 0 )
            onFinishedHeader(conn);
        else
            offset = len;
    }
    if( conn->rrs == RRS_READ_BODY && offset < len ) {
        offset += appendBodyData(conn, data + offset, len - offset);
    }
    if( conn->rrs == RRS_READ_TRAILER && offset < len ) {
        int soff = reqhdr_appendData(conn->header, data + offset, len - offset);
        if( soff >= 0 ) {
            conn->rrs = RRS_READ_FINISHED;
            offset += soff;
        }else
            offset = len;
    }
    if( rrsSav != RRS_READ_FINISHED && conn->rrs == RRS_READ_FINISHED)
        reqhdlr_requestReadCompleted(conn->handler, conn->header);
    return conn->rrs == RRS_READ_FINISHED ? offset : -1;
}

void conn_setFDAwaitingForReady(ServerConnection *conn, DataReadySelector *drs)
{
    if( conn->rrs == RRS_READ_FINISHED )
        drs_setWriteFd(drs, conn->socketFd);
    else
        drs_setReadFd(drs, conn->socketFd);
}

bool conn_processDataReady(ServerConnection *conn, DataReadySelector *drs)
{
    char buf[65536];
    int rd, wr, sysErrNo;
    bool isSuccess, freeConn;

    if( drs_clearReadFd(drs, conn->socketFd) ) {
        while( (rd = read(conn->socketFd, buf, sizeof(buf))) > 0 &&
                (wr = appendData(conn, buf, rd)) < 0 )
            ;
        if( rd < 0 ) {
            if( errno != EWOULDBLOCK )
                log_fatal("read");
        }else if( rd == 0 ) /* premature EOF */
            freeConn = true;
    }else if( drs_clearWriteFd(drs, conn->socketFd) ) {
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

