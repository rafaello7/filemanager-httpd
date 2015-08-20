#include <stdbool.h>
#include "folder.h"
#include "membuf.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


struct Folder {
    FolderEntry *entries;
    unsigned entryCount;
    unsigned entryAlloc;
};

Folder *folder_new(void)
{
    Folder *res = malloc(sizeof(Folder));

    res->entries = malloc(sizeof(FolderEntry));
    res->entryCount = 0;
    res->entryAlloc = 0;
    res->entries->fileName = NULL;
    return res;
}

void folder_addEntryChunk(Folder *folder, const DataChunk *name, bool isDir,
        unsigned mode, unsigned long long size)
{
    FolderEntry *fe;

    if( folder->entryCount == folder->entryAlloc ) {
        folder->entryAlloc = (folder->entryAlloc ? 2*folder->entryAlloc : 6)+1;
        folder->entries = realloc(folder->entries,
                (folder->entryAlloc+1) * sizeof(FolderEntry));
    }
    fe = folder->entries + folder->entryCount;
    fe->fileName = dch_dupToStr(name);
    fe->isDir = isDir;
    fe->mode = mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    fe->size = size;
    fe[1].fileName = NULL;
    ++folder->entryCount;
}

void folder_addEntry(Folder *folder, const char *name, bool isDir,
        unsigned mode, unsigned long long size)
{
    DataChunk dch;

    dch_init(&dch, name, strlen(name));
    folder_addEntryChunk(folder, &dch, isDir, mode, size);
}

static int folderEntCompare(const void *pvEnt1, const void *pvEnt2)
{
    const FolderEntry *ent1 = pvEnt1;
    const FolderEntry *ent2 = pvEnt2;

    if( ent1 ->isDir != ent2->isDir )
        return ent2->isDir - ent1->isDir;
    return strcoll(ent1->fileName, ent2->fileName);
}

void folder_sortEntries(Folder *folder)
{
    if( folder->entryCount > 1 ) {
        qsort(folder->entries, folder->entryCount, sizeof(FolderEntry),
                folderEntCompare);
    }
}

const FolderEntry *folder_getEntries(const Folder *folder)
{
    return folder->entries;
}

void folder_free(Folder *folder)
{
    int i;

    if( folder != NULL ) {
        for(i = 0; i < folder->entryCount; ++i)
            free((char*)folder->entries[i].fileName);
        free(folder->entries);
        free(folder);
    }
}

Folder *folder_loadDir(const char *dir, int *sysErrNo)
{
    DIR *d;
    struct dirent *dp;
    struct stat st;
    unsigned dirNameLen;
    Folder *folder = NULL;

    *sysErrNo = 0;
    if( (d = opendir(dir)) != NULL ) {
        MemBuf *filePathName = mb_newWithStr(dir);
        mb_ensureEndsWithSlash(filePathName);
        dirNameLen = mb_dataLen(filePathName);
        folder = folder_new();
        while( (dp = readdir(d)) != NULL ) {
            if( !strcmp(dp->d_name, ".") || ! strcmp(dp->d_name, ".."))
                continue;
            mb_setStrEnd(filePathName, dirNameLen, dp->d_name);
            if( stat(mb_data(filePathName), &st) == 0 ) {
                folder_addEntry(folder, dp->d_name, S_ISDIR(st.st_mode),
                        st.st_mode, st.st_size);
            }
        }
        folder_sortEntries(folder);
        closedir(d);
        mb_free(filePathName);
    }else{
        *sysErrNo = errno;
    }
    return folder;
}
