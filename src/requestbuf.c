#include "requestbuf.h"
#include "membuf.h"
#include <stdlib.h>
#include <string.h>


enum RequestReadState {
    RRS_READ_HEAD,
    RRS_READ_BODY,
    RRS_READ_TRAILER,
    RRS_READ_FINISHED
};

struct RequestBuf {
    enum RequestReadState rrs;
    char *request;
    const char *path;
    char **headers;
    int headerCount;
    char *chunkHdr;
    MemBuf *body;
    unsigned bodyReadLen;
};

RequestBuf *req_new(void)
{
    RequestBuf *req = malloc(sizeof(RequestBuf));

    req->rrs = RRS_READ_HEAD;
    req->request = strdup("");
    req->path = "/";
    req->headers = NULL;
    req->headerCount = 0;
    req->chunkHdr = NULL;
    req->body = NULL;
    return req;
}

const char *req_getMethod(const RequestBuf *req)
{
    return req->request;
}

const char *req_getPath(const RequestBuf *req)
{
    return req->path;
}

const char *req_getHeaderVal(const RequestBuf *req, const char *headerName)
{
    int i, len = strlen(headerName);
    const char *header;

    for(i = 0; i < req->headerCount; ++i) {
        header = req->headers[i];
        if( ! strncasecmp(header, headerName, len) && header[len] == ':' ) {
            header += len + 1;
            header += strspn(header, " \t");
            return header;
        }
    }
    return NULL;
}

const MemBuf *req_getBody(const RequestBuf *req)
{
    return req->body;
}

static void onFinishedHeader(RequestBuf *req)
{
    int isChunked = 0;
    const char *val;
    char *src, *dest, num[3];

    if( (src = strchr(req->request, ' ')) != NULL ) {
        *src++ = '\0';
        req->path = dest = src;
        while( *src && *src != ' ' ) {
            if( *src == '%' ) {
                ++src;
                num[0] = *src;
                if( *src ) {
                    ++src;
                    num[1] = *src;
                    if( *src ) ++src;
                }
                num[2] = '\0';
                *((unsigned char*)dest) = strtoul(num, NULL, 16);
            }else{
                if( dest != src )
                    *dest = *src;
                ++src;
            }
            ++dest;
        }
        *dest = '\0';
    }
    req->bodyReadLen = 0;
    if( (val = req_getHeaderVal(req, "Transfer-Encoding")) != NULL) {
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
        if( (val = req_getHeaderVal(req, "Content-Length")) != NULL )
            bodyLen = strtoul(val, NULL, 10);
        if( bodyLen > 0 ) {
            req->rrs = RRS_READ_BODY;
            req->body = mb_new();
            mb_resize(req->body, bodyLen);
        }else
            req->rrs = RRS_READ_FINISHED;
    }
}

static int appendHeaderData(RequestBuf *req, const char *data, unsigned len)
{
    const char *bol, *eol;
    char **curLoc;
    unsigned curLen, addLen, isFinish = 0;

    bol = data;
    while( ! isFinish && bol < data + len ) {
        curLoc = req->headerCount == 0 ? &req->request :
            req->headers + req->headerCount - 1;
        curLen = strlen(*curLoc);
        eol = memchr(bol, '\n', data + len - bol);
        addLen = (eol==NULL ? data+len : eol) - bol;
        *curLoc = realloc(*curLoc, curLen + addLen + 1);
        memcpy(*curLoc + curLen, bol, addLen);
        curLen += addLen;
        (*curLoc)[curLen] = '\0';
        if( eol != NULL ) {
            if( curLen > 0 && (*curLoc)[curLen-1] == '\r' )
                (*curLoc)[--curLen] = '\0';
            if( **curLoc == '\0' ) {
                isFinish = 1;
            }else{
                req->headers = realloc(req->headers,
                        (req->headerCount+1) * sizeof(char*));
                req->headers[req->headerCount] = strdup("");
                ++req->headerCount;
            }
            bol = eol + 1;
        }else{
            bol = data + len;
        }
    }
    return isFinish ? bol - data : -1;
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
        offset = appendHeaderData(req, data, len);
        if( offset >= 0 )
            onFinishedHeader(req);
        else
            offset = len;
    }
    if( req->rrs == RRS_READ_BODY && offset < len ) {
        offset += appendBodyData(req, data + offset, len - offset);
    }
    if( req->rrs == RRS_READ_TRAILER && offset < len ) {
        int soff = appendHeaderData(req, data + offset, len - offset);
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
    int i;

    free(req->request);
    for(i = 0; i < req->headerCount; ++i)
        free(req->headers[i]);
    free(req->headers);
    free(req->chunkHdr);
    mb_free(req->body);
    free(req);
}

