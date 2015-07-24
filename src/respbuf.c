#include "respbuf.h"
#include "membuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

struct RespBuf {
    int sysErrno;
    MemBuf *header;
    MemBuf *body;
};

RespBuf *resp_new(int sysErrno)
{
    RespBuf *resp;
    const char *status;

    switch(sysErrno) {
    case -405:
        status = "405 Method Not Allowed";
        break;
    case 0:
        status = "200 OK";
        break;
    case ENOENT:
        status = "404 Not Found";
        break;
    case EPERM:
    case EACCES:
        status = "403 Forbidden";
        break;
    default:
        status = "500 Internal server error";
        break;
    }
    resp = malloc(sizeof(RespBuf));
    resp->sysErrno = sysErrno;
    resp->header = mb_new();
    resp->body = mb_new();
    mb_appendStr(resp->header, "HTTP/1.1 ");
    mb_appendStr(resp->header, status);
    mb_appendStr(resp->header, "\r\n");
    return resp;
}

const char *resp_getErrMessage(RespBuf *resp)
{
    switch( resp->sysErrno ) {
    case -405:
        return "Method Not Allowed";
    case 0:
        return NULL;
    case ENOENT:
        return "Not Found";
    case EPERM:
    case EACCES:
        return "Forbidden";
    default:
        return strerror(resp->sysErrno);
    }
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

MemBuf *resp_finish(RespBuf *resp, int onlyHead)
{
    char contentLength[20];
    MemBuf *res;

    if( ! onlyHead ) {
        sprintf(contentLength, "%u", mb_dataLen(resp->body));
        resp_appendHeader(resp, "Content-Length", contentLength);
    }
    mb_appendStr(resp->header, "\r\n");
    if( ! onlyHead )
        mb_appendBuf(resp->header, resp->body);
    mb_free(resp->body);
    res = resp->header;
    free(resp);
    return res;
}

