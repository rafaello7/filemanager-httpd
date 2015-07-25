#include "folder.h"
#include "membuf.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


static int folderEntCompare(const void *pvEnt1, const void *pvEnt2)
{
    const FolderEntry *ent1 = pvEnt1;
    const FolderEntry *ent2 = pvEnt2;

    if( ent1 ->isDir != ent2->isDir )
        return ent2->isDir - ent1->isDir;
    return strcoll(ent1->fileName, ent2->fileName);
}

FolderEntry *folder_loadDir(const char *dirName, int *is_modifiable,
        int *errNum)
{
    DIR *d;
    struct dirent *dp;
    struct stat st;
    FolderEntry *res = NULL;
    int count = 0, alloc = 0, dirNameLen;

    if( (d = opendir(dirName)) != NULL ) {
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
                if( count == alloc ) {
                    alloc = alloc ? 2 * alloc : 8;
                    res = realloc(res, alloc * sizeof(FolderEntry));
                }
                res[count].fileName = strdup(dp->d_name);
                res[count].isDir = S_ISDIR(st.st_mode);
                res[count].size = st.st_size;
                ++count;
            }
        }
        closedir(d);
        mb_free(filePathName);
        qsort(res, count, sizeof(FolderEntry), folderEntCompare);
        res = realloc(res, (count+1) * sizeof(FolderEntry));
        res[count].fileName = NULL;
        *is_modifiable = access(dirName, W_OK) == 0;
    }else{
        *errNum = errno;
        *is_modifiable = 0;
    }
    return res;
}

void folder_free(FolderEntry *entries)
{
    FolderEntry *cur_ent;

    for(cur_ent = entries; cur_ent->fileName; ++cur_ent) {
        free(cur_ent->fileName);
    }
    free(entries);
}

