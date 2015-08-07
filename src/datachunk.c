#include <stdbool.h>
#include "datachunk.h"
#include <string.h>
#include <stdlib.h>


static const char gWhiteSpaces[] = " \t\n";


static bool indexOfStr(const DataChunk *dch, const char *str,
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
        return true;
    }
    return false;
}

void dch_clear(DataChunk *dch)
{
    dch->data = NULL;
    dch->len = 0;
}

void dch_init(DataChunk *dch, const char *data, unsigned len)
{
    dch->data = data;
    dch->len = len;
}

void dch_initWithStr(DataChunk *dch, const char *str)
{
    dch->data = str;
    dch->len = str ? strlen(str) : 0;
}

bool dch_shift(DataChunk *dch, unsigned size)
{
    if( dch->len < size )
        return false;
    dch->data += size;
    dch->len -= size;
    return true;
}

bool dch_shiftAfterChr(DataChunk *dch, char c)
{
    const char *dataEnd = memchr(dch->data, c, dch->len);

    if( dataEnd == NULL )
        return false;
    dch->len -= dataEnd - dch->data + 1;
    dch->data = dataEnd + 1;
    return true;
}

bool dch_shiftAfterStr(DataChunk *dch, const char *str)
{
    int idxEnd;

    if( ! indexOfStr(dch, str, NULL, &idxEnd ) )
        return false;
    dch->data += idxEnd;
    dch->len -= idxEnd;
    return true;
}

void dch_skipLeading(DataChunk *dch, const char *str)
{
    while( dch->len > 0 && *dch->data && strchr(str, *dch->data) ) {
        ++dch->data;
        --dch->len;
    }
}

void dch_trimTrailing(DataChunk *dch, const char *str)
{
    while( dch->len > 0 && dch->data[dch->len-1] &&
            strchr(str, dch->data[dch->len-1]) )
        --dch->len;
}

void dch_trimWS(DataChunk *dch)
{
    dch_skipLeading(dch, gWhiteSpaces);
    dch_trimTrailing(dch, gWhiteSpaces);
}

void dch_dirNameOf(const DataChunk *fileName, DataChunk *dirName)
{
    unsigned len = fileName->len;

    /* Trim trailing slashes if fileName is a directory, but keep root slash */
    while( len > 1 && fileName->data[len-1] == '/' )
        --len;
    /* strip last path element */
    while( len && fileName->data[len-1] != '/' )
        --len;
    /* trim trailing slashes but keep root slash */
    while( len > 1 && fileName->data[len-1] == '/' )
        --len;
    dch_init(dirName, fileName->data, len);
}

bool dch_extractTillStr(DataChunk *dch, DataChunk *subChunk, const char *str)
{
    int idxBeg, idxEnd;

    if( indexOfStr(dch, str, &idxBeg, &idxEnd ) ) {
        subChunk->data = dch->data;
        subChunk->len = idxBeg;
        dch->data += idxEnd;
        dch->len -= idxEnd;
        return true;
    }
    *subChunk = *dch;
    dch_clear(dch);
    return false;
}

bool dch_extractTillStr2(DataChunk *dch, DataChunk *subChunk,
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
                return true;
            }
            if( ! dch_shift(&dch2, idxBeg+1) )
                break;
        }
    }
    *subChunk = *dch;
    dch_clear(dch);
    return false;
}

bool dch_extractTillStrStripWS(DataChunk *dch, DataChunk *subChunk,
        const char *str)
{
    bool res = dch_extractTillStr(dch, subChunk, str);

    if( res ) {
        dch_skipLeading(dch, gWhiteSpaces);
        dch_trimTrailing(subChunk, gWhiteSpaces);
    }
    return res;
}

bool dch_extractTillWS(DataChunk *dch, DataChunk *subChunk)
{
    dch_skipLeading(dch, gWhiteSpaces);
    if( dch->len > 0 ) {
        subChunk->data = dch->data;
        while( dch->len > 0 &&
            (*dch->data == '\0' || strchr(gWhiteSpaces, *dch->data) == NULL))
        {
            ++dch->data;
            --dch->len;
        }
        subChunk->len = dch->data - subChunk->data;
        dch_skipLeading(dch, gWhiteSpaces);
        return true;
    }
    dch_clear(subChunk);
    return false;
}

bool dch_equalsStr(const DataChunk *dch, const char *str)
{
    int len = strlen(str);

    return dch->len == len && !memcmp(dch->data, str, len);
}

bool dch_equalsStrIgnoreCase(const DataChunk *dch, const char *str)
{
    int len = strlen(str);

    return dch->len == len && !strncasecmp(dch->data, str, len);
}

bool dch_startsWithStr(const DataChunk *dch, const char *str)
{
    int len = strlen(str);

    return dch->len >= len && !memcmp(dch->data, str, len);
}

bool dch_startsWithStrIgnoreCase(const DataChunk *dch, const char *str)
{
    int len = strlen(str);

    return dch->len >= len && !strncasecmp(dch->data, str, len);
}

int dch_indexOfStr(const DataChunk *dch, const char *str)
{
    int idxBeg;

    return indexOfStr(dch, str, &idxBeg, NULL) ? idxBeg : -1;
}

unsigned dch_endOfSpan(const DataChunk *dch, unsigned idxFrom, char c)
{
    while( idxFrom < dch->len && dch->data[idxFrom] == c )
        ++idxFrom;
    return idxFrom;
}

unsigned dch_endOfCSpan(const DataChunk *dch, unsigned idxFrom, char c)
{
    while( idxFrom < dch->len && dch->data[idxFrom] != c )
        ++idxFrom;
    return idxFrom;
}

char *dch_dupToStr(const DataChunk *dch)
{
    char *res = malloc(dch->len + 1);
    memcpy(res, dch->data, dch->len);
    res[dch->len] = '\0';
    return res;
}

bool dch_toUInt(const DataChunk *dch, int base, unsigned *result)
{
    char *cpy, *endptr;
    bool ret = false;
    unsigned res;
   
    if( dch->len > 0 ) {
        cpy = dch_dupToStr(dch);
        res = strtoul(cpy, &endptr, base);
        if( endptr != cpy ) {
            *result = res;
            ret = true;
        }
        free(cpy);
    }
    return ret;
}

