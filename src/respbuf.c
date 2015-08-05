#include <stdbool.h>
#include "respbuf.h"
#include "membuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

struct RespBuf {
    const char*statusStr;
    MemBuf *header;
    MemBuf *body;
};

RespBuf *resp_new(HttpStatus status)
{
    RespBuf *resp;
    const char *statusStr;

    switch( status ) {
    case HTTP_200_OK:
        statusStr = "200 OK";
        break;
    case HTTP_301_MOVED_PERMANENTLY:
        statusStr = "301 Moved Permanently";
        break;
    case HTTP_401_UNAUTHORIZED:
        statusStr = "401 Unauthorized";
        break;
    case HTTP_403_FORBIDDEN:
        statusStr = "403 Forbidden";
        break;
    case HTTP_404_NOT_FOUND:
        statusStr = "404 Not Found";
        break;
    case HTTP_405_METHOD_NOT_ALLOWED:
        statusStr = "405 Method Not Allowed";
        break;
    default:
        statusStr = "500 Internal Server Error";
        break;
    }
    resp = malloc(sizeof(RespBuf));
    resp->statusStr = statusStr;
    resp->header = mb_new();
    resp->body = mb_new();
    mb_appendStr(resp->header, "HTTP/1.1 ");
    mb_appendStr(resp->header, statusStr);
    mb_appendStr(resp->header, "\r\n");
    return resp;
}

const char *resp_getErrorMessage(RespBuf *resp)
{
    return resp->statusStr;
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

MemBuf *resp_finish(RespBuf *resp, bool onlyHead)
{
    char contentLength[20];
    MemBuf *res;

    if( ! onlyHead ) {
        sprintf(contentLength, "%u", mb_dataLen(resp->body));
        resp_appendHeader(resp, "Content-Length", contentLength);
    }
    mb_appendStr(resp->header, "\r\n");
    if( ! onlyHead )
        mb_appendData(resp->header,
                mb_data(resp->body), mb_dataLen(resp->body));
    mb_free(resp->body);
    res = resp->header;
    free(resp);
    return res;
}

