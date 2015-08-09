#include <stdbool.h>
#include "requestbuf.h"
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
    MemBuf *body;
    unsigned bodyReadLen;
};

RequestBuf *req_new(void)
{
    RequestBuf *req = malloc(sizeof(RequestBuf));

    req->rrs = RRS_READ_HEAD;
    req->header = reqhdr_new();
    req->chunkHdr = NULL;
    req->body = NULL;
    req->bodyReadLen = 0;
    return req;
}

const RequestHeader *req_getHeader(const RequestBuf *req)
{
    return req->header;
}

const MemBuf *req_getBody(const RequestBuf *req)
{
    return req->body;
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
    if( isChunked ) {
        req->chunkHdr = strdup("");
        req->rrs = RRS_READ_BODY;
        req->body = mb_new();
    }else{
        int bodyLen = 0;
        if( (val = reqhdr_getHeaderVal(req->header, "Content-Length")) != NULL)
            bodyLen = strtoul(val, NULL, 10);
        if( bodyLen > 0 ) {
            req->rrs = RRS_READ_BODY;
            req->body = mb_new();
            mb_resize(req->body, bodyLen);
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
        int bodyLen = mb_dataLen(req->body);
        if( req->bodyReadLen < bodyLen ) {
            addLen = data + len - bol;
            if( req->bodyReadLen + addLen > bodyLen )
                addLen = bodyLen - req->bodyReadLen;
            mb_setData(req->body, req->bodyReadLen,
                    bol, addLen);
            req->bodyReadLen += addLen;
            bol += addLen;
        }
        if( req->bodyReadLen == bodyLen ) {
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
                        mb_resize(req->body, /* strip CRLF */
                            mb_dataLen(req->body) - 2);
                        req->rrs = RRS_READ_TRAILER;
                    }else if( mb_dataLen(req->body) == 0 ) {
                        mb_resize(req->body, addLen+2);
                    }else{
                        mb_resize(req->body,
                            mb_dataLen(req->body) + addLen);
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
    return req->rrs == RRS_READ_FINISHED ? offset : -1;
}

void req_free(RequestBuf *req)
{
    if( req != NULL ) {
        reqhdr_free(req->header);
        free(req->chunkHdr);
        mb_free(req->body);
        free(req);
    }
}

