#include <stdbool.h>
#include "requestbuf.h"
#include "reqhandler.h"
#include "membuf.h"
#include "auth.h"
#include "fmlog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


enum RequestReadState {
    RRS_READ_HEAD,
    RRS_READ_BODY,
    RRS_READ_TRAILER,
    RRS_READ_FINISHED
};

struct RequestBuf {
    enum RequestReadState rrs;
    RequestHeader *header;
    char *chunkHdr;
    RequestHandler *handler;
    unsigned long long bodyLen;
    unsigned long long bodyReadLen;
};

RequestBuf *req_new(void)
{
    RequestBuf *req = malloc(sizeof(RequestBuf));

    req->rrs = RRS_READ_HEAD;
    req->header = reqhdr_new();
    req->chunkHdr = NULL;
    req->handler = NULL;
    req->bodyLen = 0;
    req->bodyReadLen = 0;
    return req;
}

static void onFinishedHeader(RequestBuf *req)
{
    int isChunked = 0;
    const char *val;

    if( (val = reqhdr_getHeaderVal(req->header, "Transfer-Encoding")) != NULL) {
        while( ! isChunked && val ) {
            val += strspn(val, ", \t");
            isChunked = !strncasecmp(val, "chunked", 7);
            val = strchr(val, ',');
        }
    }
    req->handler = reqhdlr_new(req->header);
    if( isChunked ) {
        req->chunkHdr = strdup("");
        req->rrs = RRS_READ_BODY;
    }else{
        if( (val = reqhdr_getHeaderVal(req->header, "Content-Length")) != NULL)
            req->bodyLen = strtoull(val, NULL, 10);
        if( req->bodyLen > 0 ) {
            req->rrs = RRS_READ_BODY;
        }else
            req->rrs = RRS_READ_FINISHED;
    }
}

static int appendBodyData(RequestBuf *req, const char *data, unsigned len)
{
    const char *bol, *eol;
    unsigned curLen, addLen;

    bol = data;
    while( req->rrs == RRS_READ_BODY && bol < data + len ) {
        if( req->bodyReadLen < req->bodyLen ) {
            addLen = data + len - bol;
            if( req->bodyReadLen + addLen > req->bodyLen )
                addLen = req->bodyLen - req->bodyReadLen;
            reqhdlr_consumeBodyBytes(req->handler, bol, addLen);
            req->bodyReadLen += addLen;
            bol += addLen;
        }
        if( req->bodyReadLen == req->bodyLen ) {
            if( req->chunkHdr != NULL ) {
                curLen = strlen(req->chunkHdr);
                eol = memchr(bol, '\n', len - (bol-data));
                addLen = (eol==NULL ? data+len : eol) - bol;
                req->chunkHdr = realloc(req->chunkHdr,
                        curLen + addLen + 1);
                memcpy(req->chunkHdr + curLen, bol, addLen);
                curLen += addLen;
                req->chunkHdr[curLen] = '\0';
                if( eol != NULL ) {
                    addLen = strtoul(req->chunkHdr, NULL, 16);
                    if( addLen == 0 ) {
                        req->bodyLen -= 2;      /* strip CRLF */
                        req->rrs = RRS_READ_TRAILER;
                    }else if( req->bodyLen == 0 ) {
                        req->bodyLen = addLen+2;
                    }else{
                        req->bodyLen += addLen;
                        req->bodyReadLen -= 2; /* strip CRLF*/
                    }
                    bol = eol + 1;
                    req->chunkHdr[0] = '\0';
                }else{
                    bol = data + len;
                }
            }else{
                req->rrs = RRS_READ_FINISHED;
            }
        }
    }
    return data+len - bol;
}

int req_appendData(RequestBuf *req, const char *data, unsigned len)
{
    int offset = 0;
    enum RequestReadState rrsSav = req->rrs;

    if( req->rrs == RRS_READ_HEAD ) {
        offset = reqhdr_appendData(req->header, data, len);
        if( offset >= 0 )
            onFinishedHeader(req);
        else
            offset = len;
    }
    if( req->rrs == RRS_READ_BODY && offset < len ) {
        offset += appendBodyData(req, data + offset, len - offset);
    }
    if( req->rrs == RRS_READ_TRAILER && offset < len ) {
        int soff = reqhdr_appendData(req->header, data + offset, len - offset);
        if( soff >= 0 ) {
            req->rrs = RRS_READ_FINISHED;
            offset += soff;
        }else
            offset = len;
    }
    if( (rrsSav == RRS_READ_HEAD || rrsSav == RRS_READ_BODY) &&
        (req->rrs == RRS_READ_TRAILER || req->rrs == RRS_READ_FINISHED))
    {
        reqhdlr_bodyBytesComplete(req->handler);
    }
    return req->rrs == RRS_READ_FINISHED ? offset : -1;
}

bool req_isReadFinished(const RequestBuf *req)
{
    return req->rrs == RRS_READ_FINISHED;
}

bool req_emitResponseBytes(RequestBuf *req, int fd, int *sysErrNo)
{
    return reqhdlr_emitResponseBytes(req->handler, fd, sysErrNo);
}

void req_free(RequestBuf *req)
{
    if( req != NULL ) {
        reqhdr_free(req->header);
        free(req->chunkHdr);
        reqhdlr_free(req->handler);
        free(req);
    }
}

