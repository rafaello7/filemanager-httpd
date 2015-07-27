#ifndef FOLDER_H
#define FOLDER_H

#include "datachunk.h"

typedef struct {
    const char *fileName;
    int isDir;
    unsigned size;
} FolderEntry;

typedef struct Folder Folder;


/* Creates new empty folder
 */
Folder *folder_new(void);


/* Adds new entry.
 */
void folder_addEntry(Folder*, const char *name, int isDir, unsigned size);
void folder_addEntryChunk(Folder*, const DataChunk *name, int isDir,
        unsigned size);


/* Sort entries in folder. Entries are sorted as follows: folders first,
 * sorted alphabetically, then files, sorted alphabetically.
 */
void folder_sortEntries(Folder*);


/* Returns array of entries. The array is terminated with entry having
 * fileName set to NULL.
 */
const FolderEntry *folder_getEntries(const Folder*);


/* Convenience method; loads system directory contents into folder.
 * The returned entries are already sorted.
 */
Folder *folder_loadDir(const char *dirName, int *is_modifiable, int *errNum);


/* Ends use of folder.
 */
void folder_free(Folder*);


#endif /* FOLDER_H */
