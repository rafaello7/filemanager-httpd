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
    const char *path, *query, *version;
    char **headers;
    int headerCount;    /* number of complete headers; the headerCount-th item
                         * is incomplete.
                         * Value -1 means the request line is incomplete.
                         */
    enum LoginState loginState;
};

RequestHeader *reqhdr_new(void)
{
    RequestHeader *req = malloc(sizeof(RequestHeader));

    req->request = strdup("");
    req->path = req->query = NULL;
    req->version = "";
    req->headers = NULL;
    req->headerCount = -1;
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

const char *reqhdr_getQuery(const RequestHeader *req)
{
    return req->query;
}

const char *reqhdr_getVersion(const RequestHeader *req)
{
    return req->version;
}

bool reqhdr_getHeaderAt(const RequestHeader *req, unsigned idx,
        const char **nameBuf, const char **valueBuf)
{
    const char *headerLine;

    if( (int)idx >= req->headerCount )
        return false;
    headerLine = req->headers[idx];
    *nameBuf = headerLine;
    *valueBuf = headerLine + strlen(headerLine) + 1;
    *valueBuf += strspn(*valueBuf, " \t");
    return true;
}

const char *reqhdr_getHeaderVal(const RequestHeader *req,
        const char *headerName)
{
    unsigned i;
    const char *headerLine;

    for(i = 0; i < req->headerCount; ++i) {
        headerLine = req->headers[i];
        if( ! strcasecmp(headerLine, headerName) ) {
            headerLine += strlen(headerName) + 1;
            headerLine += strspn(headerLine, " \t");
            return headerLine;
        }
    }
    return NULL;
}

bool reqhdr_isChunkedTransferEncoding(const RequestHeader *req)
{
    const char *val;
    bool isChunked = false;

    if( (val = reqhdr_getHeaderVal(req, "Transfer-Encoding")) != NULL) {
        while( ! isChunked && val ) {
            val += strspn(val, ", \t");
            isChunked = !strncasecmp(val, "chunked", 7);
            val = strchr(val, ',');
        }
    }
    return isChunked;
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
    char *src, *dest, num[3], csrc;

    /* req->request example: "GET /some%20dir/file?query HTTP/1.1" */
    if( (src = strchr(req->request, ' ')) != NULL ) {
        *src++ = '\0';  /* request method terminate with '\0' */
        req->path = dest = src;
        /* decode URL ("%20" etc. entities)*/
        while( (csrc = *src) != '\0' && csrc != ' ' && csrc != '?' ) {
            if( csrc == '%' ) {
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
                    *dest = csrc;
                ++src;
            }
            ++dest;
        }
        *dest = '\0';   /* end of path */
        if( csrc == '?' ) {
            /* query string (do not decode) */
            req->query = ++src;
            src += strcspn(src, " ");
            csrc = *src;
            *src = '\0';    /* end of query */
        }
        if( csrc == ' ' && !strncmp(src+1, "HTTP/", 5) )
            req->version = src + 6;
    }else{
        req->path = "/";
    }
}

static void checkAuthorization(RequestHeader *req)
{
    static const char authHeaderName[] = "Authorization";
    const char *auth = req->headers[req->headerCount];

    if(req->loginState == LS_LOGGED_OUT && !strcasecmp(auth, authHeaderName)) {
        auth += sizeof(authHeaderName);
        req->loginState = auth_isClientAuthorized(auth, reqhdr_getMethod(req)) ?
            LS_LOGGED_IN : LS_LOGIN_FAIL;
        log_debug("Authorization: %s", auth);
    }
}

int reqhdr_appendData(RequestHeader *req, const char *data, unsigned len)
{
    const char *bol, *eol;
    char **curLoc, *colon;
    unsigned curLen, addLen;
    bool isFinish = false;

    bol = data;
    while( ! isFinish && bol < data + len ) {
        curLoc = req->headerCount == -1 ? &req->request :
            req->headers + req->headerCount;
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
            if( **curLoc == '\0' ) {    /* empty line */
                isFinish = req->headerCount >= 0;
            }else{
                if( req->headerCount >= 0 ) {
                    /* replace colon with '\0' */
                    colon = strchr(req->headers[req->headerCount], ':');
                    if( colon != NULL ) {
                        *colon = '\0';
                        checkAuthorization(req);
                    }else{
                        log_debug("No colon in header line (line ignored): %s",
                                req->headers[req->headerCount]);
                        free(req->headers[req->headerCount]);
                        --req->headerCount;
                    }
                }else
                    decodeRequestStartLine(req);
                /* start next header line */
                ++req->headerCount;
                req->headers = realloc(req->headers,
                        (req->headerCount+1) * sizeof(char*));
                req->headers[req->headerCount] = strdup("");
            }
            bol = eol + 1;
        }else{
            bol = data + len;
        }
    }
    return isFinish ? bol - data : -1;
}

void reqhdr_free(RequestHeader *req)
{
    int i;

    if( req != NULL ) {
        free(req->request);
        for(i = 0; i <= req->headerCount; ++i)
            free(req->headers[i]);
        free(req->headers);
        free(req);
    }
}

