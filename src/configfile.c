#include "configfile.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


static Share *gShares;


void config_parse(void)
{
    static const char config_fname[] = "/etc/filemanager-httpd.conf";
    FILE *fp;
    char buf[1024];
    char *val;
    int count = 0;

    if( gShares == NULL ) {
        if( (fp = fopen(config_fname, "r")) != NULL ) {
            while( fgets(buf, sizeof(buf), fp) != NULL ) {
                if( buf[0] == '\0' || buf[0] == '#' )
                    continue;
                buf[strlen(buf)-1] = '\0';
                if( (val = strchr(buf, '=')) != NULL ) {
                    *val++ = '\0';
                    gShares = realloc(gShares, (count+1) * sizeof(Share));
                    gShares[count].urlpath = strdup(buf);
                    gShares[count].syspath = strdup(val);
                    ++count;
                }
            }
        }else{
            gShares = malloc(sizeof(Share));
            gShares->urlpath = "/";
            gShares->syspath = "/";
            count = 1;
        }
        gShares = realloc(gShares, (count+1) * sizeof(Share));
        gShares[count].urlpath = NULL;
        gShares[count].syspath = NULL;
    }
}

const Share *config_getShareForPath(const char *path)
{
    const Share *cur, *best = NULL;
    int pathLen, bestLen = -1;

    pathLen = strlen(path);
    if( pathLen > 0 && path[pathLen-1] == '/' )
        --pathLen;
    for(cur = gShares; cur->urlpath; ++cur) {
        int urlLen = strlen(cur->urlpath);
        while( urlLen > 0 && cur->urlpath[urlLen-1] == '/' )
            --urlLen;
        if( urlLen > pathLen || urlLen <= bestLen )
            continue;
        if( !strncmp(path, cur->urlpath, urlLen) &&
            (urlLen == pathLen || path[urlLen] == '/') )
        {
            best = cur;
            bestLen = urlLen;
        }
    }
    return best;
}

Folder *config_getSubSharesForPathAsFolder(const char *path)
{
    Folder *folder = NULL;
    const Share *cur;
    int pathLen;
    DataChunk dchPath, ent;

    pathLen = strlen(path);
    if( pathLen > 0 && path[pathLen-1] == '/' )
        --pathLen;
    for(cur = gShares; cur->urlpath; ++cur) {
        if( !strncmp(path, cur->urlpath, pathLen) &&
                cur->urlpath[pathLen] == '/')
        {
            dchInitWithStr(&dchPath, cur->urlpath + pathLen + 1);
            dchSkipInitial(&dchPath, "/");
            if( dchPath.len > 0 ) {
                dchExtractTillStr(&dchPath, &ent, "/");
                if( folder == NULL )
                    folder = folder_new();
                folder_addEntryChunk(folder, &ent, 1, 0);
            }
        }
    }
    return folder;
}

