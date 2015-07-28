#include "datachunk.h"
#include <string.h>
#include <stdlib.h>


static const char gWhiteSpaces[] = " \t\n";


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

void dch_Clear(DataChunk *dch)
{
    dch->data = NULL;
    dch->len = 0;
}

void dch_Init(DataChunk *dch, const char *data, unsigned len)
{
    dch->data = data;
    dch->len = len;
}

void dch_InitWithStr(DataChunk *dch, const char *str)
{
    dch->data = str;
    dch->len = strlen(str);
}

int dch_Shift(DataChunk *dch, unsigned size)
{
    if( dch->len < size )
        return 0;
    dch->data += size;
    dch->len -= size;
    return 1;
}

int dch_ShiftAfterChr(DataChunk *dch, char c)
{
    const char *dataEnd = memchr(dch->data, c, dch->len);

    if( dataEnd == NULL )
        return 0;
    dch->len -= dataEnd - dch->data + 1;
    dch->data = dataEnd + 1;
    return 1;
}

int dch_ShiftAfterStr(DataChunk *dch, const char *str)
{
    int idxEnd;

    if( ! indexOfStr(dch, str, NULL, &idxEnd ) )
        return 0;
    dch->data += idxEnd;
    dch->len -= idxEnd;
    return 1;
}

void dch_SkipLeading(DataChunk *dch, const char *str)
{
    while( dch->len > 0 && *dch->data && strchr(str, *dch->data) ) {
        ++dch->data;
        --dch->len;
    }
}

void dch_TrimTrailing(DataChunk *dch, const char *str)
{
    while( dch->len > 0 && dch->data[dch->len-1] &&
            strchr(str, dch->data[dch->len-1]) )
        --dch->len;
}

void dch_TrimWS(DataChunk *dch)
{
    dch_SkipLeading(dch, gWhiteSpaces);
    dch_TrimTrailing(dch, gWhiteSpaces);
}

int dch_ExtractTillStr(DataChunk *dch, DataChunk *subChunk, const char *str)
{
    int idxBeg, idxEnd;

    if( indexOfStr(dch, str, &idxBeg, &idxEnd ) ) {
        subChunk->data = dch->data;
        subChunk->len = idxBeg;
        dch->data += idxEnd;
        dch->len -= idxEnd;
        return 1;
    }
    *subChunk = *dch;
    dch_Clear(dch);
    return 0;
}

int dch_ExtractTillStr2(DataChunk *dch, DataChunk *subChunk,
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
            if( ! dch_Shift(&dch2, idxBeg+1) )
                break;
        }
    }
    *subChunk = *dch;
    dch_Clear(dch);
    return 0;
}

int dch_ExtractTillStrStripWS(DataChunk *dch, DataChunk *subChunk,
        const char *str)
{
    int res = dch_ExtractTillStr(dch, subChunk, str);

    if( res ) {
        dch_SkipLeading(dch, gWhiteSpaces);
        dch_TrimTrailing(subChunk, gWhiteSpaces);
    }
    return res;
}

int dch_ExtractTillWS(DataChunk *dch, DataChunk *subChunk)
{
    dch_SkipLeading(dch, gWhiteSpaces);
    if( dch->len > 0 ) {
        subChunk->data = dch->data;
        while( dch->len > 0 &&
            (*dch->data == '\0' || strchr(gWhiteSpaces, *dch->data) == NULL))
        {
            ++dch->data;
            --dch->len;
        }
        subChunk->len = dch->data - subChunk->data;
        dch_SkipLeading(dch, gWhiteSpaces);
        return 1;
    }
    dch_Clear(subChunk);
    return 0;
}

int dch_EqualsStr(const DataChunk *dch, const char *str)
{
    int len = strlen(str);

    return dch->len == len && !memcmp(dch->data, str, len);
}

int dch_StartsWithStr(const DataChunk *dch, const char *str)
{
    int len = strlen(str);

    return dch->len >= len && !memcmp(dch->data, str, len);
}

int dch_IndexOfStr(const DataChunk *dch, const char *str)
{
    int idxBeg;

    return indexOfStr(dch, str, &idxBeg, NULL) ? idxBeg : -1;
}

char *dch_DupToStr(const DataChunk *dch)
{
    char *res = NULL;
   
    if( dch->len > 0 ) {
        res = malloc(dch->len + 1);
        memcpy(res, dch->data, dch->len);
        res[dch->len] = '\0';
    }
    return res;
}

int dch_ToUInt(const DataChunk *dch, int base, unsigned *result)
{
    char *cpy, *endptr;
    int ret = 0;
    unsigned res;
   
    if( dch->len > 0 ) {
        cpy = dch_DupToStr(dch);
        res = strtoul(cpy, &endptr, base);
        if( endptr != cpy ) {
            *result = res;
            ret = 1;
        }
        free(cpy);
    }
    return ret;
}

