#ifndef SERVEFILE_H
#define SERVEFILE_H

#include "datachunk.h"

typedef struct {
    const char *fileName;
    int isDir;
    unsigned size;
} FolderEntry;

typedef struct ServeFile ServeFile;


/* Creates new empty folder
 */
ServeFile *sf_new(const char *urlPath, const char *sysPath, int isFolder);


int sf_isFolder(const ServeFile*);
const char *sf_getUrlPath(const ServeFile*);
const char *sf_getSysPath(const ServeFile*);


void sf_setIndexFile(ServeFile*, const char*);


/* If some file was set using sf_setIndexFile, the file is returned.
 * Otherwise, if the serve file is not a folder, the file sysPath
 * is returned. Otherwise the function returns NULL.
 */
const char *sf_getIndexFile(const ServeFile*);


/* Returns non-zero when file/directory defined by sysPath may be modified.
 */
int sf_isModifiable(const ServeFile*);


/* Adds new entry.
 */
void sf_addEntry(ServeFile*, const char *name, int isDir, unsigned size);
void sf_addEntryChunk(ServeFile*, const DataChunk *name, int isDir,
        unsigned size);


/* Sort entries in folder. Entries are sorted as follows: folders first,
 * sorted alphabetically, then files, sorted alphabetically.
 */
void sf_sortEntries(ServeFile*);


/* Returns array of entries. The array is terminated with entry having
 * fileName set to NULL.
 */
const FolderEntry *sf_getEntries(const ServeFile*);


/* Convenience method; loads system directory contents into folder.
 * The returned entries are already sorted.
 */
ServeFile *sf_loadDir(const char *dirName, int *is_modifiable, int *errNum);


/* Ends use of folder.
 */
void sf_free(ServeFile*);


#endif /* SERVEFILE_H */
