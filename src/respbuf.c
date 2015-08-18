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

void resp_enqFile(RespBuf *resp, int fileDesc)
{
    if( resp->fileDesc != -1 )
        close(resp->fileDesc);
    resp->fileDesc = fileDesc;
}

void resp_appendStrL(RespBuf *resp, const char *str1, 
        const char *str2, ...)
{
    va_list args;

    mb_appendStr(resp->body, str1);
    if( str2 != NULL ) {
        va_start(args, str2);
        while( str2 != NULL ) {
            mb_appendStr(resp->body, str2);
            str2 = va_arg(args, const char*);
        }
        va_end(args);
    }
}

void resp_appendDataEscapeHtml(RespBuf *resp, const char *data, unsigned len)
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

void resp_appendStrEscapeHtml(RespBuf *resp, const char *str)
{
    resp_appendDataEscapeHtml(resp, str, strlen(str));
}

void resp_appendChunkEscapeHtml(RespBuf *resp, const DataChunk *dch)
{
    resp_appendDataEscapeHtml(resp, dch->data, dch->len);
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

