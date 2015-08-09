#include <stdbool.h>
#include "requestheader.h"
#include "membuf.h"
#include "auth.h"
#include "fmlog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


struct RequestHeader {
    char *request;
    const char *path;
    char **headers;
    unsigned headerCount;
    enum LoginState loginState;
};

RequestHeader *reqhdr_new(void)
{
    RequestHeader *req = malloc(sizeof(RequestHeader));

    req->request = strdup("");
    req->path = NULL;
    req->headers = NULL;
    req->headerCount = 0;
    req->loginState = LS_LOGGED_OUT;
    return req;
}

const char *reqhdr_getMethod(const RequestHeader *req)
{
    return req->request;
}

const char *reqhdr_getPath(const RequestHeader *req)
{
    return req->path;
}

static const char *getHeaderVal(char *const*headers, unsigned headerCount,
        const char *headerName)
{
    unsigned i, len = strlen(headerName);
    const char *header;

    for(i = 0; i < headerCount; ++i) {
        header = headers[i];
        if( ! strncasecmp(header, headerName, len) && header[len] == ':' ) {
            header += len + 1;
            header += strspn(header, " \t");
            return header;
        }
    }
    return NULL;
}

const char *reqhdr_getHeaderVal(const RequestHeader *req,
        const char *headerName)
{
    return getHeaderVal(req->headers, req->headerCount, headerName);
}

enum LoginState reqhdr_getLoginState(const RequestHeader *req)
{
    return req->loginState;
}

bool reqhdr_isWorthPuttingLogOnButton(const RequestHeader *req)
{
    return req->loginState == LS_LOGGED_OUT &&
        config_givesLoginMorePrivileges();
}

bool reqhdr_isActionAllowed(const RequestHeader *req, enum PrivilegedAction pa)
{
    return config_isActionAllowed(pa, req->loginState == LS_LOGGED_IN);
}

static void decodeRequestStartLine(RequestHeader *req)
{
    char *src, *dest, num[3];

    if( (src = strchr(req->request, ' ')) != NULL ) {
        *src++ = '\0';  /* request method terminate with '\0' */
        req->path = dest = src;
        /* decode URL ("%20" etc. entities)*/
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
    }else{
        req->path = "/";
    }
}

static int appendHeaderData(RequestHeader *req, const char *data, unsigned len)
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

int reqhdr_appendData(RequestHeader *req, const char *data, unsigned len)
{
    int i, offset, oldHeaderCount;

    oldHeaderCount = req->headerCount ? req->headerCount : 1;
    offset = appendHeaderData(req, data, len);
    if( req->path == NULL && req->headerCount > 0 )
        decodeRequestStartLine(req);
    if( req->loginState == LS_LOGGED_OUT && req->headerCount > oldHeaderCount )
    {
        const char *auth = getHeaderVal(req->headers + oldHeaderCount - 1,
                req->headerCount - oldHeaderCount, "Authorization");
        if( auth != NULL ) {
            req->loginState =
                auth_isClientAuthorized(auth, reqhdr_getMethod(req)) ?
                    LS_LOGGED_IN : LS_LOGIN_FAIL;
            log_debug("Authorization: %s", auth);
        }
    }
    if( offset >= 0 ) {
        log_debug("request: %s %s", req->request, req->path);
        if( log_isLevel(2) ) {
            for(i = 0; i < req->headerCount; ++i)
                log_debug("%s", req->headers[i]);
        }
    }
    return offset;
}

void reqhdr_free(RequestHeader *req)
{
    int i;

    free(req->request);
    for(i = 0; i < req->headerCount; ++i)
        free(req->headers[i]);
    free(req->headers);
    free(req);
}

