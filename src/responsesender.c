#include <stdbool.h>
#include "responsesender.h"
#include "fmlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


struct ResponseSender {
    MemBuf *header;
    MemBuf *body;
    int fileDesc;
    unsigned dataSize;
    unsigned dataOffset;    /* index of first unwritten byte in data */
    long long nbytes;       /* total number of bytes to write; -1 for
                             * "chunked" Transfer-Encoding */
};

ResponseSender *rsndr_new(MemBuf *header, MemBuf *body, int fileDesc)
{
    ResponseSender *rsndr;
    char contentLength[40];

    rsndr = malloc(sizeof(ResponseSender));
    rsndr->header = header;
    rsndr->body = body;
    rsndr->dataOffset = 0;
    rsndr->nbytes = 0;
    if( body ) {
        struct stat st;
        if( fileDesc != -1 ) {
            if( fstat(fileDesc, &st) != -1 ) {
                if( S_ISREG(st.st_mode) )
                    rsndr->nbytes = st.st_size;
                else{
                    rsndr->nbytes = -1;
                }
            }else
                log_error("rsndr_new: fstat");
        }
        if( rsndr->nbytes >= 0 ) {
            sprintf(contentLength, "Content-Length: %llu\r\n",
                    mb_dataLen(body) + rsndr->nbytes);
            mb_appendStr(header, contentLength);
        }else{
            mb_appendStr(header, "Transfer-Encoding: chunked\r\n");
        }
    }
    if( fileDesc != -1 && rsndr->nbytes == 0 ) {
        close(fileDesc);
        fileDesc = -1;
    }
    rsndr->fileDesc = fileDesc;
    if( log_isLevel(2) )
        log_debug("==> response: %s", mb_data(header));
    else
        log_debug("response: %.*s",
                strcspn(mb_data(header), "\r\n"), mb_data(header));
    mb_appendStr(header, "\r\n");
    if( body ) {
        if( rsndr->nbytes == -1 && mb_dataLen(body) > 0 ) {
            /* prepare first chunk: the chunk begin appending to header */
            sprintf(contentLength, "%x\r\n", mb_dataLen(body));
            mb_appendStr(header, contentLength);
            mb_appendStr(body, "\r\n");
        }
        rsndr->dataSize = mb_dataLen(body);
    }else
        rsndr->dataSize = 0;
    return rsndr;
}

static void fillBuffer(ResponseSender *rsndr, DataReadySelector *drs)
{
    int toFill, rd, filledCount;
    char chunkHeader[10];

    if( rsndr->nbytes >= 0 ) {
        toFill = rsndr->nbytes < 65536 ? rsndr->nbytes : 65536;
        if( mb_dataLen(rsndr->body) < toFill )
            mb_resize(rsndr->body, toFill);
        filledCount = 0;
        if( rsndr->fileDesc >= 0 ) {
            while( filledCount < toFill && (rd = mb_readFile(rsndr->body,
                    rsndr->fileDesc, filledCount, toFill - filledCount)) > 0)
                filledCount += rd;
            if( filledCount < toFill ) {
                if( rd < 0 && errno == EWOULDBLOCK ) {
                    if( filledCount == 0 )
                        drs_setReadFd(drs, rsndr->fileDesc);
                }else{
                    if( rd < 0 )
                        log_error("fillBuffer");
                    close(rsndr->fileDesc);
                    rsndr->fileDesc = -1;
                }
            }
        }
        if( filledCount < toFill && rsndr->fileDesc < 0 ) {
            mb_fillWithZeros(rsndr->body, filledCount, toFill - filledCount);
            filledCount = toFill;
        }
        rsndr->nbytes -= filledCount;
        rsndr->dataSize = filledCount;
        rsndr->dataOffset = 0;
    }else{
        toFill = 65546;
        if( mb_dataLen(rsndr->body) < toFill + 7 )
            mb_resize(rsndr->body, toFill + 7);
        filledCount = 10;   /* making space for chunk header */
        while( filledCount < toFill && (rd = mb_readFile(rsndr->body,
                    rsndr->fileDesc, filledCount, toFill - filledCount)) > 0)
            filledCount += rd;
        if( filledCount < toFill && (rd == 0 || errno != EWOULDBLOCK) ) {
            if( rd < 0 )
                log_error("fillBuffer");
            close(rsndr->fileDesc);
            rsndr->fileDesc = -1;
        }
        if( filledCount > 10 ) {
            rd = sprintf(chunkHeader, "%x\r\n", filledCount-10);
            mb_setData(rsndr->body, 10-rd, chunkHeader, rd);
            rsndr->dataOffset = 10 - rd;
            mb_setData(rsndr->body, filledCount, "\r\n", 2);
            filledCount += 2;
        }else{
            rsndr->dataOffset = 0;
            filledCount = 0;
        }
        if( rsndr->fileDesc == -1 ) {
            /* adding last chunk and empty trailer */
            mb_setData(rsndr->body, filledCount, "0\r\n\r\n", 5);
            filledCount += 5;
            rsndr->nbytes = 0;
        }
        if( filledCount == 0 )
            drs_setReadFd(drs, rsndr->fileDesc);
        rsndr->dataSize = filledCount;
    }
}

bool rsndr_send(ResponseSender *rsndr, int fd, DataReadySelector *drs)
{
    int wr;

    if( rsndr->header != NULL ) {
        while( rsndr->dataOffset < mb_dataLen(rsndr->header) &&
                (wr = write(fd, mb_data(rsndr->header) + rsndr->dataOffset,
                        mb_dataLen(rsndr->header) - rsndr->dataOffset)) >= 0)
        {
            rsndr->dataOffset += wr;
        }
        if( rsndr->dataOffset < mb_dataLen(rsndr->header) ) {
            if( errno == EWOULDBLOCK ) {
                drs_setWriteFd(drs, fd);
                return false;
            }else{
                if( errno != ECONNRESET && errno != EPIPE )
                    log_error("connected socket write failed");
                return true;
            }
        }else{
            mb_free(rsndr->header);
            rsndr->header = NULL;
            rsndr->dataOffset = 0;
        }
    }
    if( rsndr->body != NULL ) {
        while( true ) {
            while( rsndr->dataOffset < rsndr->dataSize &&
                    (wr = write(fd, mb_data(rsndr->body) + rsndr->dataOffset,
                            rsndr->dataSize - rsndr->dataOffset)) >= 0)
            {
                rsndr->dataOffset += wr;
            }
            if( rsndr->dataOffset < rsndr->dataSize ) {
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
            }else if( rsndr->nbytes != 0 ) {
                fillBuffer(rsndr, drs);
                if( rsndr->dataSize == 0 )
                    return false;
            }else
                return true;
        }
    }
    return true;
}

void rsndr_free(ResponseSender *rsndr)
{
    if( rsndr != NULL ) {
        mb_free(rsndr->header);
        mb_free(rsndr->body);
        if( rsndr->fileDesc != -1 )
            close(rsndr->fileDesc);
        free(rsndr);
    }
}

