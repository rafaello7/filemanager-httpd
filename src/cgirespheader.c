#include <stdbool.h>
#include "cgirespheader.h"
#include "membuf.h"
#include "auth.h"
#include "fmlog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


struct CgiRespHeader {
    char **headers;
    unsigned headerCount; /* number of complete headers; the headerCount-th
                           * item is incomplete. */
};

CgiRespHeader *cgirhdr_new(void)
{
    CgiRespHeader *req = malloc(sizeof(CgiRespHeader));

    req->headers = malloc(sizeof(char*));
    req->headers[0] = strdup("");
    req->headerCount = 0;
    return req;
}

bool cgirhdr_getHeaderAt(const CgiRespHeader *req, unsigned idx,
        const char **nameBuf, const char **valueBuf)
{
    const char *headerLine;

    if( (int)idx >= req->headerCount )
        return false;
    headerLine = req->headers[idx];
    *nameBuf = headerLine;
    *valueBuf = headerLine + strlen(headerLine) + 1;
    return true;
}

const char *cgirhdr_getHeaderVal(const CgiRespHeader *req,
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

int cgirhdr_appendData(CgiRespHeader *req, const char *data, unsigned len)
{
    const char *bol, *eol;
    char **curLoc, *colon;
    unsigned curLen, addLen, i;
    bool isFinish = false;

    bol = data;
    while( ! isFinish && bol < data + len ) {
        curLoc = req->headers + req->headerCount;
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
                isFinish = true;
            }else{
                /* replace colon with '\0' */
                colon = strchr(req->headers[req->headerCount], ':');
                if( colon != NULL ) {
                    *colon = '\0';
                }else{
                    log_debug("No colon in CGI header line (line ignored): %s",
                            req->headers[req->headerCount]);
                    free(req->headers[req->headerCount]);
                    --req->headerCount;
                }
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
    if( isFinish && log_isLevel(2) ) {
        log_debug("CGI response headers:");
        for(i = 0; i < req->headerCount; ++i)
            log_debug("  %s:%s", req->headers[i],
                    req->headers[i] + strlen(req->headers[i]) + 1);
    }
    return isFinish ? bol - data : -1;
}

void cgirhdr_free(CgiRespHeader *req)
{
    int i;

    for(i = 0; i <= req->headerCount; ++i)
        free(req->headers[i]);
    free(req->headers);
    free(req);
}

