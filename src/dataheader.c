#include <stdbool.h>
#include "dataheader.h"
#include "membuf.h"
#include "auth.h"
#include "fmlog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


struct DataHeader {
    char **lines;
    unsigned lineCount;   /* number of complete header lines; the
                           * lineCount-th item is incomplete. */
};

DataHeader *datahdr_new(void)
{
    DataHeader *hdr = malloc(sizeof(DataHeader));

    hdr->lines = malloc(sizeof(char*));
    hdr->lines[0] = strdup("");
    hdr->lineCount = 0;
    return hdr;
}

bool datahdr_getHeaderLineAt(const DataHeader *hdr, unsigned idx,
        const char **nameBuf, const char **valueBuf)
{
    const char *headerLine;

    if( (int)idx >= hdr->lineCount )
        return false;
    headerLine = hdr->lines[idx];
    *nameBuf = headerLine;
    *valueBuf = headerLine + strlen(headerLine) + 1;
    return true;
}

const char *datahdr_getHeaderVal(const DataHeader *hdr,
        const char *headerName)
{
    unsigned i;
    const char *headerLine;

    for(i = 0; i < hdr->lineCount; ++i) {
        headerLine = hdr->lines[i];
        if( ! strcasecmp(headerLine, headerName) ) {
            headerLine += strlen(headerName) + 1;
            headerLine += strspn(headerLine, " \t");
            return headerLine;
        }
    }
    return NULL;
}

int datahdr_appendData(DataHeader *hdr, const char *data, unsigned len,
        const char *debugMsgLoc)
{
    const char *bol, *eol;
    char **curLoc, *colon;
    unsigned curLen, addLen, i;
    bool isFinish = false;

    bol = data;
    while( ! isFinish && bol < data + len ) {
        curLoc = hdr->lines + hdr->lineCount;
        curLen = strlen(*curLoc);
        eol = memchr(bol, '\n', data + len - bol);
        addLen = (eol==NULL ? data+len : eol) - bol;
        *curLoc = realloc(*curLoc, curLen + addLen + 1);
        memcpy(*curLoc + curLen, bol, addLen);
        curLen += addLen;
        (*curLoc)[curLen] = '\0';
        if( eol != NULL ) {
            if( curLen > 0 && (*curLoc)[curLen-1] == '\r' )
                (*curLoc)[--curLen] = '\0';
            if( **curLoc == '\0' ) {    /* empty line */
                isFinish = true;
            }else{
                /* replace colon with '\0' */
                colon = strchr(hdr->lines[hdr->lineCount], ':');
                if( colon != NULL ) {
                    *colon = '\0';
                }else{
                    log_debug("No colon in %s header line (line ignored): %s",
                            debugMsgLoc, hdr->lines[hdr->lineCount]);
                    free(hdr->lines[hdr->lineCount]);
                    --hdr->lineCount;
                }
                /* start next header line */
                ++hdr->lineCount;
                hdr->lines = realloc(hdr->lines,
                        (hdr->lineCount+1) * sizeof(char*));
                hdr->lines[hdr->lineCount] = strdup("");
            }
            bol = eol + 1;
        }else{
            bol = data + len;
        }
    }
    if( isFinish && log_isLevel(2) ) {
        log_debug("%s header lines:", debugMsgLoc);
        for(i = 0; i < hdr->lineCount; ++i)
            log_debug("  %s:%s", hdr->lines[i],
                    hdr->lines[i] + strlen(hdr->lines[i]) + 1);
    }
    return isFinish ? bol - data : -1;
}

void datahdr_free(DataHeader *hdr)
{
    int i;

    for(i = 0; i <= hdr->lineCount; ++i)
        free(hdr->lines[i]);
    free(hdr->lines);
    free(hdr);
}

