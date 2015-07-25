#ifndef FOLDER_H
#define FOLDER_H


typedef struct {
    char *fileName;
    int isDir;
    unsigned size;
} FolderEntry;

FolderEntry *folder_loadDir(const char *dirName, int *is_modifiable,
        int *errNum);
void folder_free(FolderEntry*);

#endif /* FOLDER_H */
