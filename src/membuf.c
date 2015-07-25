#include "membuf.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

struct MemBuf {
    char *data;
    unsigned dataLen;
};

MemBuf *mb_new(void)
{
    MemBuf *res = malloc(sizeof(MemBuf));

    res->data = NULL;
    res->dataLen = 0;
    return res;
}

void mb_appendData(MemBuf *mb, const char *data, unsigned dataLen)
{
    mb->data = realloc(mb->data, mb->dataLen + dataLen);
    memcpy(mb->data + mb->dataLen, data, dataLen);
    mb->dataLen += dataLen;
}

void mb_appendBuf(MemBuf *mb, const MemBuf *mbSrc)
{
    mb_appendData(mb, mbSrc->data, mbSrc->dataLen);
}

void mb_appendStr(MemBuf *mb, const char *str)
{
    mb_appendData(mb, str, strlen(str));
}

void mb_resize(MemBuf *mb, unsigned newSize)
{
    if( newSize == 0 ) {
        free(mb->data);
        mb->data = NULL;
    }else{
        mb->data = realloc(mb->data, newSize);
    }
    mb->dataLen = newSize;
}

const void *mb_data(const MemBuf *mb)
{
    return mb->data;
}

void mb_setData(MemBuf *mb, unsigned offset, const char *data, unsigned len)
{
    if( offset + len > mb->dataLen ) {
        fprintf(stderr, "mb_setData error: offset+len exceeds buffer size,"
                "offset=%u, len=%u, bufsize=%u\n", offset, len, mb->dataLen);
        abort();
    }
    memcpy(mb->data + offset, data, len);
}

void mb_setDataExtend(MemBuf *mb, unsigned offset, const char *data,
        unsigned len)
{
    if( offset + len > mb->dataLen ) {
        mb->dataLen = offset + len;
        mb->data = realloc(mb->data, mb->dataLen);
    }
    memcpy(mb->data + offset, data, len);
}

unsigned mb_dataLen(const MemBuf *mb)
{
    return mb->dataLen;
}

void mb_free(MemBuf *mb)
{
    if( mb != NULL ) {
        free(mb->data);
        free(mb);
    }
}

