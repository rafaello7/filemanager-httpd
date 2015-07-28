#include "servefile.h"
#include "membuf.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


struct ServeFile {
    char *urlPath, *sysPath;
    int isFolder;
    FolderEntry *entries;
    unsigned entryCount;
    unsigned entryAlloc;
};

ServeFile *sf_new(const char *urlPath, const char *sysPath, int isFolder)
{
    ServeFile *res = malloc(sizeof(ServeFile));

    res->urlPath = urlPath == NULL ? NULL : strdup(urlPath);
    res->sysPath = sysPath == NULL ? NULL : strdup(sysPath);
    res->isFolder = isFolder;
    res->entries = malloc(sizeof(FolderEntry));
    res->entries->fileName = NULL;
    res->entryCount = 0;
    res->entryAlloc = 0;
    return res;
}

int sf_isFolder(const ServeFile *sf)
{
    return sf->isFolder;
}

const char *sf_getUrlPath(const ServeFile *sf)
{
    return sf->urlPath;
}

const char *sf_getSysPath(const ServeFile *sf)
{
    return sf->sysPath;
}

int sf_isModifiable(const ServeFile *sf)
{
    return sf->sysPath == NULL ? 0 : access(sf->sysPath, W_OK) == 0;
}

void sf_addEntryChunk(ServeFile *sf, const DataChunk *name, int isDir,
        unsigned size)
{
    FolderEntry *fe;

    if( sf->entryCount == sf->entryAlloc ) {
        sf->entryAlloc = (sf->entryAlloc ? 2*sf->entryAlloc : 6)+1;
        sf->entries = realloc(sf->entries,
                (sf->entryAlloc+1) * sizeof(FolderEntry));
    }
    fe = sf->entries + sf->entryCount;
    fe->fileName = dch_DupToStr(name);
    fe->isDir = isDir;
    fe->size = size;
    fe[1].fileName = NULL;
    ++sf->entryCount;
}

void sf_addEntry(ServeFile *sf, const char *name, int isDir, unsigned size)
{
    DataChunk dch;

    dch_Init(&dch, name, strlen(name));
    sf_addEntryChunk(sf, &dch, isDir, size);
}

static int folderEntCompare(const void *pvEnt1, const void *pvEnt2)
{
    const FolderEntry *ent1 = pvEnt1;
    const FolderEntry *ent2 = pvEnt2;

    if( ent1 ->isDir != ent2->isDir )
        return ent2->isDir - ent1->isDir;
    return strcoll(ent1->fileName, ent2->fileName);
}

void sf_sortEntries(ServeFile *sf)
{
    if( sf->entryCount > 1 ) {
        qsort(sf->entries, sf->entryCount, sizeof(FolderEntry),
                folderEntCompare);
    }
}

const FolderEntry *sf_getEntries(const ServeFile *sf)
{
    return sf->entries;
}

void sf_free(ServeFile *sf)
{
    int i;

    for(i = 0; i < sf->entryCount; ++i)
        free((char*)sf->entries[i].fileName);
    free(sf->entries);
    free(sf->sysPath);
    free(sf->urlPath);
    free(sf);
}

