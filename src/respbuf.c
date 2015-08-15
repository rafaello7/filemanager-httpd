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
    const char*statusStr;
    MemBuf *header;
    MemBuf *body;
    int fileDesc;
    unsigned dataSize;
    unsigned dataOffset;    /* index of first unwritten byte in data */
    long long nbytes;       /* total number of bytes to write; -1 for
                             * "chunked" Transfer-Encoding */
};

RespBuf *resp_new(HttpStatus status, bool onlyHead)
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
    resp->body = onlyHead ? NULL : mb_new();
    resp->fileDesc = -1;
    resp->dataOffset = 0;
    resp->dataSize = 0;
    resp->nbytes = 0;
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

RespBuf *resp_finish(RespBuf *resp)
{
    char contentLength[20];

    resp_appendHeader(resp, "Connection", "close");
    resp_appendHeader(resp, "Server", "filemanager-httpd");
    if( resp->body ) {
        struct stat st;
        if( resp->fileDesc != -1 ) {
            if( fstat(resp->fileDesc, &st) != -1 ) {
                if( S_ISREG(st.st_mode) )
                    resp->nbytes = st.st_size;
                else{
                    resp->nbytes = -1;
                }
            }else
                log_error("resp_finish: fstat");
        }
        if( resp->nbytes >= 0 ) {
            sprintf(contentLength, "%llu",
                    mb_dataLen(resp->body) + resp->nbytes);
            resp_appendHeader(resp, "Content-Length", contentLength);
        }else{
            resp_appendHeader(resp, "Transfer-Encoding", "chunked");
        }
    }
    if( resp->fileDesc != -1 && resp->nbytes == 0 ) {
        close(resp->fileDesc);
        resp->fileDesc = -1;
    }
    if( log_isLevel(2) )
        log_debug("==> response: %s", mb_data(resp->header));
    else
        log_debug("response: %s", resp->statusStr);
    mb_appendStr(resp->header, "\r\n");
    if( resp->body ) {
        if( resp->nbytes == -1 && mb_dataLen(resp->body) > 0 ) {
            /* prepare first chunk: chunk begin append to header */
            sprintf(contentLength, "%x\r\n", mb_dataLen(resp->body));
            mb_appendStr(resp->header, contentLength);
            mb_appendStr(resp->body, "\r\n");
        }
        resp->dataSize = mb_dataLen(resp->body);
    }
    return resp;
}

static void fillBuffer(RespBuf *resp, DataReadySelector *drs)
{
    int toFill, rd, filledCount = 0;
    char chunkHeader[10];

    if( resp->nbytes > 0 ) {
        toFill = resp->nbytes < 65536 ? resp->nbytes : 65536;
        if( mb_dataLen(resp->body) < toFill )
            mb_resize(resp->body, toFill);
        while( filledCount < toFill && resp->fileDesc != -1 ) {
            rd = mb_readFile(resp->body, resp->fileDesc, filledCount,
                    toFill - filledCount);
            if( rd > 0 ) {
                filledCount += rd;
            }else{
                if( rd < 0 ) {
                    if( errno == EWOULDBLOCK ) {
                        if( filledCount == 0 )
                            drs_setReadFd(drs, resp->fileDesc);
                        resp->dataSize = filledCount;
                        return;
                    }
                    log_error("fillBuffer");
                }
                close(resp->fileDesc);
                resp->fileDesc = -1;
            }
        }
        if( filledCount < toFill )
            mb_fillWithZeros(resp->body, filledCount, toFill - filledCount);
        resp->nbytes -= toFill;
        resp->dataSize = toFill;
    }else{
        toFill = 65546;
        if( mb_dataLen(resp->body) < toFill + 7 )
            mb_resize(resp->body, toFill + 7);
        filledCount = 10;   /* space for chunk header */
        while( filledCount < toFill && resp->fileDesc != -1 ) {
            rd = mb_readFile(resp->body, resp->fileDesc, filledCount,
                    toFill - filledCount);
            if( rd > 0 ) {
                filledCount += rd;
            }else{
                if( rd < 0 ) {
                    if( errno == EWOULDBLOCK )
                        break;
                    log_error("fillBuffer");
                }
                close(resp->fileDesc);
                resp->fileDesc = -1;
            }
        }
        if( filledCount > 10 ) {
            rd = sprintf(chunkHeader, "%x\r\n", filledCount-10);
            mb_setData(resp->body, 10-rd, chunkHeader, rd);
            resp->dataOffset = 10 - rd;
            mb_setData(resp->body, filledCount, "\r\n", 2);
            filledCount += 2;
        }else{
            resp->dataOffset = 0;
            filledCount = 0;
        }
        if( resp->fileDesc == -1 ) {
            mb_setData(resp->body, filledCount, "0\r\n\r\n", 5);
            filledCount += 5;
            resp->nbytes = 0;
        }
        if( filledCount == 0 )
            drs_setReadFd(drs, resp->fileDesc);
        resp->dataSize = filledCount;
    }
}

bool resp_write(RespBuf *resp, int fd, DataReadySelector *drs)
{
    int wr;

    if( resp->header != NULL ) {
        while( resp->dataOffset < mb_dataLen(resp->header) &&
                (wr = write(fd, mb_data(resp->header) + resp->dataOffset,
                            mb_dataLen(resp->header) - resp->dataOffset)) >= 0)
        {
            resp->dataOffset += wr;
        }
        if( resp->dataOffset < mb_dataLen(resp->header) ) {
            if( errno == EWOULDBLOCK ) {
                drs_setWriteFd(drs, fd);
                return false;
            }else{
                if( errno != ECONNRESET && errno != EPIPE )
                    log_error("connected socket write failed");
                return true;
            }
        }else{
            mb_free(resp->header);
            resp->header = NULL;
            resp->dataOffset = 0;
        }
    }
    if( resp->body != NULL ) {
        while( true ) {
            while( resp->dataOffset < resp->dataSize &&
                    (wr = write(fd, mb_data(resp->body) + resp->dataOffset,
                            resp->dataSize - resp->dataOffset)) >= 0)
            {
                resp->dataOffset += wr;
            }
            if( resp->dataOffset < resp->dataSize ) {
                /* ECONNRESET occurs when peer has closed connection
                 * without receiving all data; similar EPIPE */
                if( errno == EWOULDBLOCK ) {
                    drs_setWriteFd(drs, fd);
                    return false;
                }else{
                    if( errno != ECONNRESET && errno != EPIPE )
                        log_error("connected socket write failed");
                    return true;
                }
            }else if( resp->nbytes != 0 ) {
                fillBuffer(resp, drs);
                if( resp->dataSize == 0 )
                    return false;
            }else
                return true;
        }
    }
    return true;
}

void resp_free(RespBuf *resp)
{
    if( resp != NULL ) {
        mb_free(resp->header);
        mb_free(resp->body);
        if( resp->fileDesc != -1 )
            close(resp->fileDesc);
        free(resp);
    }
}

