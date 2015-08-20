#ifndef FOLDER_H
#define FOLDER_H

#include "datachunk.h"

typedef struct {
    const char *fileName;
    bool isDir;
    unsigned mode;              /* rwx permissions, like st_mode */
    unsigned long long size;
} FolderEntry;

typedef struct Folder Folder;


/* Creates a new folder.
 */
Folder *folder_new(void);


/* Adds a new entry to directory listing.
 */
void folder_addEntry(Folder*, const char *name, bool isDir,
        unsigned mode, unsigned long long size);
void folder_addEntryChunk(Folder*, const DataChunk *name, bool isDir,
        unsigned mode, unsigned long long size);


/* Sorts entries in directory listing. Entries are sorted as follows:
 * folders first, sorted alphabetically, then files, sorted alphabetically.
 */
void folder_sortEntries(Folder*);


/* Returns array of entries. The array is terminated with entry having
 * fileName set to NULL.
 */
const FolderEntry *folder_getEntries(const Folder*);


/* Ends use of the object.
 */
void folder_free(Folder*);


/* Loads the specified directory contents into folder.
 */
Folder *folder_loadDir(const char *dir, int *sysErrNo);


#endif /* FOLDER_H */
