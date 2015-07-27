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
    res->entries->fileName = NULL;
    res->entryCount = 0;
    res->entryAlloc = 0;
    return res;
}

void folder_addEntryChunk(Folder *folder, const DataChunk *name, int isDir,
        unsigned size)
{
    FolderEntry *fe;

    if( folder->entryCount == folder->entryAlloc ) {
        folder->entryAlloc = (folder->entryAlloc ? 2*folder->entryAlloc : 6)+1;
        folder->entries = realloc(folder->entries,
                (folder->entryAlloc+1) * sizeof(FolderEntry));
    }
    fe = folder->entries + folder->entryCount;
    fe->fileName = dchDupToStr(name);
    fe->isDir = isDir;
    fe->size = size;
    fe[1].fileName = NULL;
    ++folder->entryCount;
}

void folder_addEntry(Folder *folder, const char *name, int isDir, unsigned size)
{
    DataChunk dch;

    dchInit(&dch, name, strlen(name));
    folder_addEntryChunk(folder, &dch, isDir, size);
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
    qsort(folder->entries, folder->entryCount, sizeof(FolderEntry),
            folderEntCompare);
}

const FolderEntry *folder_getEntries(const Folder *folder)
{
    return folder->entries;
}

Folder *folder_loadDir(const char *dirName, int *is_modifiable, int *errNum)
{
    DIR *d;
    struct dirent *dp;
    struct stat st;
    Folder *res = NULL;
    int dirNameLen;

    if( (d = opendir(dirName)) != NULL ) {
        res = folder_new();
        MemBuf *filePathName = mb_new();
        dirNameLen = strlen(dirName);
        mb_setDataExtend(filePathName, 0, dirName, dirNameLen);
        if( dirName[dirNameLen-1] != '/' )
            mb_setDataExtend(filePathName, dirNameLen++, "/", 1);
        while( (dp = readdir(d)) != NULL ) {
            if( ! strcmp(dp->d_name, ".") || ! strcmp(dp->d_name, "..") )
                continue;
            mb_setDataExtend(filePathName, dirNameLen, dp->d_name,
                    strlen(dp->d_name) + 1);
            if( stat(mb_data(filePathName), &st) == 0 ) {
                folder_addEntry(res, dp->d_name, S_ISDIR(st.st_mode),
                        st.st_size);
            }
        }
        closedir(d);
        mb_free(filePathName);
        folder_sortEntries(res);
        *is_modifiable = access(dirName, W_OK) == 0;
    }else{
        *errNum = errno;
        *is_modifiable = 0;
    }
    return res;
}

void folder_free(Folder *folder)
{
    int i;

    for(i = 0; i < folder->entryCount; ++i)
        free((char*)folder->entries[i].fileName);
    free(folder->entries);
    free(folder);
}

