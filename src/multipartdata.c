#include <stdbool.h>
#include "multipartdata.h"
#include <stdlib.h>
#include <string.h>


enum ParsePosition {
    PP_BODY,            /* outside boundary line (or the boundary string
                         * matches partially) */
    PP_GOT_BOUNDARY,    /* in boundary line, next character will be first
                         * one after boundary string */
    PP_GOT_WS,          /* in boundary line, after boundary string and some
                         * white spaces (and possibly hyphens) */
};

/* See rfc2045, rfc2046 */
struct MultipartData {
    MemBuf *boundaryDelimiter;
    char *destDir;
    enum ParsePosition parsePos;
    unsigned delimMatchPart; /* when PP_BODY: number of bytes from previous
                              * data matching boundary delimiter;
                              * other parse positions: must be 0 */
    ContentPart **parts;
    unsigned partCount;
};

MultipartData *mpdata_new(const char *boundaryDelimiter, const char *destDir)
{
    MultipartData *mpdata = malloc(sizeof(MultipartData));

    mpdata->boundaryDelimiter = mb_newWithStr("\r\n--");
    mb_appendStr(mpdata->boundaryDelimiter, boundaryDelimiter);
    mpdata->destDir = destDir ? strdup(destDir) : NULL;
    mpdata->parsePos = PP_BODY;
    mpdata->delimMatchPart = 2; /* body may start with boundary
                                 * without inital CRLF */
    mpdata->parts = NULL;
    mpdata->partCount = 0;      /* starting with preamble */
    return mpdata;
}

void mpdata_appendData(MultipartData *mpdata, const char *data, unsigned len)
{
    const char *const dataEnd = data + len, *delim, *partEnd;
    unsigned delimLen;

    if( mpdata->boundaryDelimiter == NULL ) /* after last part (in epilogue) */
        return;
    delim = mb_data(mpdata->boundaryDelimiter);
    delimLen = mb_dataLen(mpdata->boundaryDelimiter);
    if( mpdata->delimMatchPart > 0 ) {
        unsigned delimRmdrLen = delimLen - mpdata->delimMatchPart;
        if( memcmp(data, delim + mpdata->delimMatchPart,
                delimRmdrLen < len ?  delimRmdrLen : len))
        {
            /* Not the delimiter.
             * Fortunately no further part of the boundary string may match
             * the initial portion of the boundary string...
             */
            if( mpdata->partCount > 0 ) {
                cpart_appendData(mpdata->parts[mpdata->partCount-1],
                        delim, mpdata->delimMatchPart);
            }
            mpdata->delimMatchPart = 0;
        }else if( delimRmdrLen > len ) {
            mpdata->delimMatchPart += len;
            data = dataEnd;
        }else{
            data += delimRmdrLen;
            mpdata->delimMatchPart = 0;
            mpdata->parsePos = PP_GOT_BOUNDARY;
        }
    }
    while( data != dataEnd ) {
        if( mpdata->parsePos == PP_GOT_BOUNDARY ) {
            if( *data == '-' ) { /* -- reached epilogue */
                mb_free(mpdata->boundaryDelimiter);
                mpdata->boundaryDelimiter = NULL;
                return;
            }
            mpdata->parts = realloc(mpdata->parts,
                    (mpdata->partCount+1) * sizeof(ContentPart*));
            mpdata->parts[mpdata->partCount] = cpart_new(mpdata->destDir);
            ++mpdata->partCount;
        }
        if( mpdata->parsePos != PP_BODY ) {
            while( data != dataEnd && *data != '\n' )
                ++data;
            if( data == dataEnd )
                return;
            mpdata->parsePos = PP_BODY;
            if( ++data == dataEnd )
                return;
        }
        partEnd = data;
        while( (partEnd = memchr(partEnd, '\r', dataEnd - partEnd)) != NULL ) {
            if( ! memcmp(partEnd, delim,
                    delimLen <= dataEnd-partEnd ? delimLen : dataEnd-partEnd))
                break;
            ++partEnd;
        }
        if( partEnd == NULL )
            partEnd = dataEnd;
        if( mpdata->partCount > 0 ) {
            cpart_appendData(mpdata->parts[mpdata->partCount-1],
                    data, partEnd - data);
        }
        if( dataEnd - partEnd < delimLen ) {
            data = dataEnd;
            mpdata->delimMatchPart = dataEnd - partEnd;
        }else{
            data = partEnd + delimLen;
            mpdata->parsePos = PP_GOT_BOUNDARY;
        }
    }
}

const ContentPart *mpdata_getPart(MultipartData *mpdata, unsigned partNum)
{
    return partNum < mpdata->partCount ? mpdata->parts[partNum] : NULL;
}

ContentPart *mpdata_getPartByName(MultipartData *mpdata, const char *name)
{
    unsigned i;
    ContentPart *cpart = NULL;

    for(i = 0; i < mpdata->partCount && cpart == NULL; ++i) {
        if( cpart_nameEquals(mpdata->parts[i], name) )
            cpart = mpdata->parts[i];
    }
    return cpart;
}

bool mpdata_containsPartWithName(MultipartData *mpdata, const char *name)
{
    return mpdata_getPartByName(mpdata, name) != NULL;
}

void mpdata_free(MultipartData *mpdata)
{
    unsigned i;

    if( mpdata != NULL ) {
        mb_free(mpdata->boundaryDelimiter);
        free(mpdata->destDir);
        for(i = 0; i < mpdata->partCount; ++i)
            cpart_free(mpdata->parts[i]);
        free(mpdata->parts);
        free(mpdata);
    }
}

