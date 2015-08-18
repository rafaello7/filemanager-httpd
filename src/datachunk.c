#include <stdbool.h>
#include "datachunk.h"
#include <string.h>
#include <stdlib.h>


static const char gWhiteSpaces[] = " \t\n";


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

void dch_skipLeading(DataChunk *dch, char c)
{
    while( dch->len && *dch->data == c ) {
        ++dch->data;
        --dch->len;
    }
}

void dch_trimTrailing(DataChunk *dch, char c)
{
    while( dch->len && dch->data[dch->len-1] == c )
        --dch->len;
}

static void skipLeadingWS(DataChunk *dch)
{
    while( dch->len > 0 && *dch->data && strchr(gWhiteSpaces, *dch->data) ) {
        ++dch->data;
        --dch->len;
    }
}

static void trimTrailingWS(DataChunk *dch)
{
    while( dch->len > 0 && dch->data[dch->len-1] &&
            strchr(gWhiteSpaces, dch->data[dch->len-1]) )
        --dch->len;
}

void dch_trimWS(DataChunk *dch)
{
    skipLeadingWS(dch);
    trimTrailingWS(dch);
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

bool dch_extractParam(DataChunk *paramLine, DataChunk *nameBuf,
        DataChunk *valueBuf, char delimiter)
{
    bool res;

    if( (res = dch_extractTillChr(paramLine, nameBuf, '=')) ) {
        dch_trimWS(nameBuf);
        skipLeadingWS(paramLine);
        if( paramLine->len > 0 && paramLine->data[0] == '"' ) {
            dch_shift(paramLine, 1);
            dch_extractTillChr(paramLine, valueBuf, '\"');
            dch_shiftAfterChr(paramLine, delimiter);
        }else{
            dch_extractTillChr(paramLine, valueBuf, delimiter);
            trimTrailingWS(valueBuf);
        }
    }
    return res;
}

bool dch_extractTillChr(DataChunk *dch, DataChunk *subChunk, char delimiter)
{
    const char *delimPos;
    bool res;

    delimPos = memchr(dch->data, delimiter, dch->len);
    if( (res = delimPos != NULL) ) {
        subChunk->data = dch->data;
        subChunk->len = delimPos - dch->data;
        dch->data = delimPos + 1;
        dch->len -= subChunk->len + 1;
    }else{
        *subChunk = *dch;
        dch_clear(dch);
    }
    return res;
}

bool dch_extractTillChrStripWS(DataChunk *dch, DataChunk *subChunk,
        char delimiter)
{
    bool res = dch_extractTillChr(dch, subChunk, delimiter);

    if( res ) {
        skipLeadingWS(dch);
        trimTrailingWS(subChunk);
    }
    return res;
}

bool dch_extractTillWS(DataChunk *dch, DataChunk *subChunk)
{
    skipLeadingWS(dch);
    if( dch->len > 0 ) {
        subChunk->data = dch->data;
        while( dch->len > 0 &&
            (*dch->data == '\0' || strchr(gWhiteSpaces, *dch->data) == NULL))
        {
            ++dch->data;
            --dch->len;
        }
        subChunk->len = dch->data - subChunk->data;
        skipLeadingWS(dch);
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

