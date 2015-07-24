#include "datachunk.h"
#include <string.h>
#include <stdlib.h>


static int indexOfStr(const DataChunk *dch, const char *str,
        int *idxBeg, int *idxEnd)
{
    const char *data = dch->data;
    int len = strlen(str);

    while( data - dch->data + len <= dch->len && memcmp(data, str, len) )
        ++data;
    if( data - dch->data + len <= dch->len ) {
        if( idxBeg )
            *idxBeg = data - dch->data;
        if( idxEnd )
            *idxEnd = data - dch->data + len;
        return 1;
    }
    return 0;
}

void dchClear(DataChunk *dch)
{
    dch->data = NULL;
    dch->len = 0;
}

void dchInit(DataChunk *dch, const char *data, unsigned len)
{
    dch->data = data;
    dch->len = len;
}

int dchShift(DataChunk *dch, unsigned size)
{
    if( dch->len < size )
        return 0;
    dch->data += size;
    dch->len -= size;
    return 1;
}

int dchShiftAfterChr(DataChunk *dch, char c)
{
    const char *dataEnd = memchr(dch->data, c, dch->len);

    if( dataEnd == NULL )
        return 0;
    dch->len -= dataEnd - dch->data + 1;
    dch->data = dataEnd + 1;
    return 1;
}

int dchShiftAfterStr(DataChunk *dch, const char *str)
{
    int idxEnd;

    if( ! indexOfStr(dch, str, NULL, &idxEnd ) )
        return 0;
    dch->data += idxEnd;
    dch->len -= idxEnd;
    return 1;
}

void dchSkip(DataChunk *dch, const char *str)
{
    while( dch->len > 0 && *dch->data && strchr(str, *dch->data) ) {
        ++dch->data;
        --dch->len;
    }
}

int dchExtractTillStr(DataChunk *dch, DataChunk *subChunk, const char *str)
{
    int idxBeg, idxEnd;

    if( ! indexOfStr(dch, str, &idxBeg, &idxEnd ) )
        return 0;
    subChunk->data = dch->data;
    subChunk->len = idxBeg;
    dch->data += idxEnd;
    dch->len -= idxEnd;
    return 1;
}

int dchExtractTillStr2(DataChunk *dch, DataChunk *subChunk,
        const char *str1, const char *str2)
{
    int idxBeg, idxEnd, len1 = strlen(str1);
    DataChunk dch2;

    if( dch->len >= len1 ) {
        dch2.data = dch->data + len1;
        dch2.len = dch->len - len1;
        while( indexOfStr(&dch2, str2, &idxBeg, &idxEnd ) ) {
            if( ! memcmp(dch2.data + idxBeg - len1, str1, len1) ) {
                subChunk->data = dch->data;
                subChunk->len = dch2.data - dch->data + idxBeg - len1;
                dch->data = dch2.data + idxEnd;
                dch->len = dch2.len - idxEnd;
                return 1;
            }
            if( ! dchShift(&dch2, idxBeg+1) )
                break;
        }
    }
    return 0;
}

int dchEqualsStr(const DataChunk *dch, const char *str)
{
    int len = strlen(str);

    return dch->len == len && !memcmp(dch->data, str, len);
}

int dchStartsWithStr(const DataChunk *dch, const char *str)
{
    int len = strlen(str);

    return dch->len >= len && !memcmp(dch->data, str, len);
}

int dchIndexOfStr(const DataChunk *dch, const char *str)
{
    int idxBeg;

    return indexOfStr(dch, str, &idxBeg, NULL) ? idxBeg : -1;
}

char *dchDupToStr(const DataChunk *dch)
{
    char *res = NULL;
   
    if( dch->len > 0 ) {
        res = malloc(dch->len + 1);
        memcpy(res, dch->data, dch->len);
        res[dch->len] = '\0';
    }
    return res;
}

