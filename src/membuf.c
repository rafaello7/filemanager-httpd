#include <stdbool.h>
#include "membuf.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

struct MemBuf {
    char *data;
    unsigned dataLen;
};

MemBuf *mb_new(void)
{
    MemBuf *res = malloc(sizeof(MemBuf));

    res->dataLen = 0;
    res->data = malloc(1);
    res->data[0] = '\0';
    return res;
}

void mb_newIfNull(MemBuf * *mb)
{
    if( *mb == NULL )
        *mb = mb_new();
}

MemBuf *mb_newWithStr(const char *str)
{
    int len = strlen(str);
    MemBuf *res = malloc(sizeof(MemBuf));

    res->dataLen = len;
    res->data = malloc(len+1);
    memcpy(res->data, str, len+1);
    return res;
}

void mb_appendData(MemBuf *mb, const char *data, unsigned dataLen)
{
    mb->data = realloc(mb->data, mb->dataLen + dataLen + 1);
    memcpy(mb->data + mb->dataLen, data, dataLen);
    mb->dataLen += dataLen;
    mb->data[mb->dataLen] = '\0';
}

void mb_appendStr(MemBuf *mb, const char *str)
{
    mb_appendData(mb, str, strlen(str));
}

void mb_appendStrL(MemBuf *mb, const char *str1, const char *str2, ...)
{
    va_list args;

    mb_appendStr(mb, str1);
    if( str2 != NULL ) {
        va_start(args, str2);
        while( str2 != NULL ) {
            mb_appendStr(mb, str2);
            str2 = va_arg(args, const char*);
        }
        va_end(args);
    }
}

void mb_appendChunk(MemBuf *mb, const DataChunk *dch)
{
    mb_appendData(mb, dch->data, dch->len);
}

bool mb_endsWithStr(const MemBuf *mb, const char *str)
{
    int len = strlen(str);

    return mb->dataLen >= len && !memcmp(mb->data + mb->dataLen-len, str, len);
}

void mb_resize(MemBuf *mb, unsigned newSize)
{
    mb->dataLen = newSize;
    mb->data = realloc(mb->data, newSize+1);
    mb->data[newSize] = '\0';
}

const char *mb_data(const MemBuf *mb)
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

void mb_setStrEnd(MemBuf *mb, unsigned offset, const char *str)
{
    int len = strlen(str) + 1;

    mb->dataLen = offset + len;
    mb->data = realloc(mb->data, mb->dataLen);
    memcpy(mb->data + offset, str, len);
}

int mb_readFile(MemBuf *mb, int fd, unsigned bufOffset, unsigned toRead)
{
    if( bufOffset + toRead > mb->dataLen ) {
        fprintf(stderr, "mb_readFile error: offset+toRead exceeds buffer size,"
                "offset=%u, toRead=%u, bufsize=%u\n", bufOffset, toRead,
                mb->dataLen);
        abort();
    }
    return read(fd, mb->data + bufOffset, toRead);
}

void mb_fillWithZeros(MemBuf *mb, unsigned offset, unsigned len)
{
    if( offset + len > mb->dataLen ) {
        fprintf(stderr, "mb_setData error: offset+len exceeds buffer size,"
                "offset=%u, len=%u, bufsize=%u\n", offset, len, mb->dataLen);
        abort();
    }
    memset(mb->data + offset, 0, len);
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

char *mb_unbox_free(MemBuf *mb)
{
    char *res = NULL;

    if( mb != NULL ) {
        res = mb->data;
        free(mb);
    }
    return res;
}

