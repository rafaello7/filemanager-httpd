#include <stdbool.h>
#include "respbuf.h"
#include "membuf.h"
#include "fmlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

struct RespBuf {
    MemBuf *header;
    MemBuf *body;
    int fileDesc;
};

const char *resp_cmnStatus(HttpStatus status)
{
    const char *statusStr;

    switch( status ) {
    case HTTP_200_OK:
        statusStr = "200 OK";
        break;
    case HTTP_403_FORBIDDEN:
        statusStr = "403 Forbidden";
        break;
    case HTTP_404_NOT_FOUND:
        statusStr = "404 Not Found";
        break;
    default:
        statusStr = "500 Internal Server Error";
        break;
    }
    return statusStr;
}

RespBuf *resp_new(const char *status, bool onlyHead)
{
    RespBuf *resp;

    resp = malloc(sizeof(RespBuf));
    resp->header = mb_new();
    resp->body = onlyHead ? NULL : mb_new();
    resp->fileDesc = -1;
    mb_appendStr(resp->header, "HTTP/1.1 ");
    mb_appendStr(resp->header, status);
    mb_appendStr(resp->header, "\r\n");
    return resp;
}

void resp_appendHeader(RespBuf *resp, const char *name, const char *value)
{
    mb_appendStr(resp->header, name);
    mb_appendStr(resp->header, ": ");
    mb_appendStr(resp->header, value);
    mb_appendStr(resp->header, "\r\n");
}

void resp_appendData(RespBuf *resp, const char *data, unsigned dataLen)
{
    mb_appendData(resp->body, data, dataLen);
}

void resp_appendStr(RespBuf *resp, const char *str)
{
    mb_appendStr(resp->body, str);
}

static void appendDataEscapeHtml(RespBuf *resp, const char *data, unsigned len)
{
    const char *repl, *dcur = data;

    while( len ) {
        switch( *dcur ) {
        case '"':  repl = "&quot;"; break;
        case '\'': repl = "&apos;"; break;
        case '&':  repl = "&amp;";  break;
        case '<':  repl = "&lt;";   break;
        case '>':  repl = "&gt;";   break;
        default:   repl = NULL;     break;
        }
        if( repl != NULL ) {
            if( dcur != data )
                mb_appendData(resp->body, data, dcur-data);
            mb_appendStr(resp->body, repl);
            data = dcur + 1;
        }
        ++dcur;
        --len;
    }
    if( dcur != data )
        mb_appendData(resp->body, data, dcur-data);
}

void resp_appendFmt(RespBuf *resp, const char *fmt, ...)
{
    va_list args;
    const char *fbeg, *fend, *par;
    const DataChunk *dch;
    int len;
    char c;

    va_start(args, fmt);
    fbeg = fmt;
    while( (fend = strchr(fbeg, '%')) != NULL ) {
        if( fend != fbeg )
            resp_appendData(resp, fbeg, fend-fbeg);
        c = *++fend;
        if( c == 'C' || c == 'D' || c == 'S' ) {
            if( c == 'C' ) {
                dch = va_arg(args, DataChunk*);
                par = dch->data;
                len = dch->len;
            }else{
                par = va_arg(args, const char*);
                if( c == 'D' )
                    len = va_arg(args, int);
                else
                    len = strlen(par);
            }
            appendDataEscapeHtml(resp, par, len);
        }else{
            if( c == 'R' ) {
                par = va_arg(args, const char*);
                len = strlen(par);
            }else{
                if( c != fend[-1] )
                    --fend;
                par = fend;
                len = 1;
            }
            resp_appendData(resp, par, len);
        }
        fbeg = fend + 1;
    }
    va_end(args);
    if( *fbeg )
        resp_appendStr(resp, fbeg);
}

void resp_enqFile(RespBuf *resp, int fileDesc)
{
    if( resp->fileDesc != -1 )
        close(resp->fileDesc);
    resp->fileDesc = fileDesc;
}

ResponseSender *resp_finish(RespBuf *resp)
{
    ResponseSender * rsndr;
    resp_appendHeader(resp, "Connection", "close");
    resp_appendHeader(resp, "Server", "filemanager-httpd");

    rsndr = rsndr_new(resp->header, resp->body, resp->fileDesc);
    free( resp );
    return rsndr;
}

