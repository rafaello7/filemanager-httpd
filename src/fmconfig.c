#include "fmconfig.h"
#include "cmdline.h"
#include "datachunk.h"
#include "membuf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/stat.h>


typedef struct {
    const char *urlpath;
    const char *syspath;
} Share;

static unsigned gListenPort;

/* patterns specified as "index" option in configuration file.
 */
static const char **gIndexPatterns;
static unsigned gIndexPatternCount;


/* non-zero when "index" option contains "."
 */
static unsigned gIsDirectoryListing = 1;

/* List of shares, terminated with one with urlpath set to NULL
 */
static Share *gShares;


void config_parse(void)
{
    const char *configFName;
    FILE *fp;
    char buf[1024];
    DataChunk dchLine, dchName, dchPatt;
    int shareCount = 0, lineNo = 0;

    if( gShares == NULL ) {
        configFName = cmdline_getConfigFileName();
        if( (fp = fopen(configFName, "r")) != NULL ) {
            gIsDirectoryListing = 0;
            while( fgets(buf, sizeof(buf), fp) != NULL ) {
                ++lineNo;
                dch_InitWithStr(&dchLine, buf);
                dch_TrimWS(&dchLine);
                if( dchLine.len == 0 || *dchLine.data == '#' )
                    continue;
                if( dch_ExtractTillStrStripWS(&dchLine, &dchName, "=") ) {
                    if( dch_StartsWithStr(&dchName, "/") ) {
                        gShares = realloc(gShares,
                                (shareCount+1) * sizeof(Share));
                        gShares[shareCount].urlpath = dch_DupToStr(&dchName);
                        gShares[shareCount].syspath = dch_DupToStr(&dchLine);
                        ++shareCount;
                    }else if( dch_EqualsStr(&dchName, "index") ) {
                        while( dch_ExtractTillWS(&dchLine, &dchPatt) ) {
                            if( dch_EqualsStr(&dchPatt, ".") )
                                gIsDirectoryListing = 1;
                            else{
                                gIndexPatterns = realloc(gIndexPatterns,
                                        (gIndexPatternCount+1) *
                                        sizeof(const char*));
                                gIndexPatterns[gIndexPatternCount++] =
                                    dch_DupToStr(&dchPatt);
                            }
                        }
                    }else if( dch_EqualsStr(&dchName, "port") ) {
                        if( ! dch_ToUInt(&dchLine, 0, &gListenPort) )
                            fprintf(stderr, "%s:%d warning: unrecognized port",
                                    configFName, lineNo);
                    }else{
                        fprintf(stderr, "%s:%d warning: unrecognized option "
                                "\"%.*s\", ignored\n", configFName, lineNo,
                                dchName.len, dchName.data);
                    }
                }else{
                    fprintf(stderr, "%s:%d warning: missing '='; "
                            "line ignored\n", configFName, lineNo);
                }
            }
        }else{
            fprintf(stderr, "unable to open configuration file %s: %s\n",
                    configFName, strerror(errno));
        }
        if( shareCount == 0 ) {
            gShares = realloc(gShares, (shareCount+1) * sizeof(Share));
            gShares[shareCount].urlpath = "/";
            gShares[shareCount].syspath = "/var/www/html";
            ++shareCount;
        }
        gShares = realloc(gShares, (shareCount+1) * sizeof(Share));
        gShares[shareCount].urlpath = NULL;
        gShares[shareCount].syspath = NULL;
        if( gListenPort == 0 ) {
            gListenPort = geteuid() == 0 ? 80 : 8000;
        }
        if( gIndexPatternCount == 0 )
            gIsDirectoryListing = 1;
    }
}

unsigned config_getListenPort(void)
{
    return gListenPort;
}

char *config_getSysPathForUrlPath(const char *urlPath)
{
    const Share *cur, *best = NULL;
    int urlPathLen, sysPathLen, bestShLen = -1;

    urlPathLen = strlen(urlPath);
    while( urlPathLen > 0 && urlPath[urlPathLen-1] == '/' )
        --urlPathLen;
    for(cur = gShares; cur->urlpath; ++cur) {
        int shLen = strlen(cur->urlpath);
        while( shLen > 0 && cur->urlpath[shLen-1] == '/' )
            --shLen;
        if( shLen > urlPathLen || shLen <= bestShLen )
            continue;
        if( !strncmp(urlPath, cur->urlpath, shLen) &&
            (shLen == urlPathLen || urlPath[shLen] == '/') )
        {
            best = cur;
            bestShLen = shLen;
        }
    }
    if( best != NULL ) {
        MemBuf *filePathName = mb_new();
        sysPathLen = strlen(best->syspath);
        while( sysPathLen > 0 && best->syspath[sysPathLen-1] == '/' )
            --sysPathLen;
        mb_appendData(filePathName, best->syspath, sysPathLen);
        if( urlPathLen > bestShLen ) {
            mb_appendData(filePathName, urlPath + bestShLen,
                    urlPathLen - bestShLen);
        }else if( sysPathLen == 0 )
            mb_appendData(filePathName, "/", 1);
        mb_appendData(filePathName, "", 1);
        return mb_unbox_free(filePathName);
    }
    return NULL;
}

ServeFile *getSubSharesForPath(const char *urlPath)
{
    ServeFile *sf = NULL;
    const Share *cur;
    int pathLen;
    DataChunk dchPath, ent;

    pathLen = strlen(urlPath);
    while( pathLen > 0 && urlPath[pathLen-1] == '/' )
        --pathLen;
    for(cur = gShares; cur->urlpath; ++cur) {
        if( !strncmp(urlPath, cur->urlpath, pathLen) &&
                cur->urlpath[pathLen] == '/')
        {
            dch_InitWithStr(&dchPath, cur->urlpath + pathLen + 1);
            dch_SkipLeading(&dchPath, "/");
            if( dchPath.len > 0 ) {
                dch_ExtractTillStr(&dchPath, &ent, "/");
                if( sf == NULL )
                    sf = sf_new(urlPath, NULL, 1);
                sf_addEntryChunk(sf, &ent, 1, 0);
            }
        }
    }
    return sf;
}

ServeFile *config_getServeFile(const char *urlPath, int *sysErrNo)
{
    DIR *d;
    struct dirent *dp;
    struct stat st;
    char *sysPath;
    unsigned dirNameLen, matchIdx, bestMatchIdx = gIndexPatternCount;
    MemBuf *bestPath;
    ServeFile *sf = NULL;

    *sysErrNo = ENOENT;
    sysPath = config_getSysPathForUrlPath(urlPath);
    if( sysPath != NULL ) {
        if( (d = opendir(sysPath)) != NULL ) {
            bestPath = mb_new();
            MemBuf *filePathName = mb_new();
            mb_appendStr(filePathName, sysPath);
            if( sysPath[mb_dataLen(filePathName)-1] != '/' )
                mb_appendStr(filePathName, "/");
            dirNameLen = mb_dataLen(filePathName);
            while( bestMatchIdx > 0 && (dp = readdir(d)) != NULL ) {
                if( ! strcmp(dp->d_name, ".") || ! strcmp(dp->d_name, "..") )
                    continue;
                mb_setDataExtend(filePathName, dirNameLen, dp->d_name,
                        strlen(dp->d_name) + 1);
                for( matchIdx = 0; matchIdx < bestMatchIdx; ++matchIdx ) {
                    if( fnmatch(gIndexPatterns[matchIdx], mb_data(filePathName),
                                FNM_PATHNAME | FNM_PERIOD) == 0 )
                        break;
                }
                if( matchIdx < bestMatchIdx &&
                            stat(mb_data(filePathName), &st) == 0 &&
                            ! S_ISDIR(st.st_mode) )
                    mb_setDataExtend(bestPath, 0, mb_data(filePathName),
                            mb_dataLen(filePathName));
            }
            if( mb_dataLen(bestPath) > 0 ) {
                sf = sf_new(urlPath, mb_data(bestPath), 0);
            }else if( gIsDirectoryListing ) {
                rewinddir(d);
                sf = sf_new(urlPath, sysPath, 1);
                while( (dp = readdir(d)) != NULL ) {
                    if( !strcmp(dp->d_name, ".") || ! strcmp(dp->d_name, ".."))
                        continue;
                    mb_setDataExtend(filePathName, dirNameLen, dp->d_name,
                            strlen(dp->d_name) + 1);
                    if( stat(mb_data(filePathName), &st) == 0 ) {
                        sf_addEntry(sf, dp->d_name, S_ISDIR(st.st_mode),
                                st.st_size);
                    }
                }
                sf_sortEntries(sf);
            }
            closedir(d);
            mb_free(filePathName);
            mb_free(bestPath);
        }else if( errno == ENOTDIR ) {
            sf = sf_new(urlPath, sysPath, 0);
        }else{
            *sysErrNo = errno;
        }
        free(sysPath);
    }else{
        sf = getSubSharesForPath(urlPath);
    }
    return sf;
}

