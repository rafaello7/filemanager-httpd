#ifndef SERVEFILE_H
#define SERVEFILE_H

#include "datachunk.h"

typedef struct {
    const char *fileName;
    int isDir;
    unsigned size;
} FolderEntry;

typedef struct ServeFile ServeFile;


/* Creates a new serve file.
 * Parameters:
 *      urlPath     - the file URL path
 *      sysPath     - the file path on file system
 *      isFolder    - whether is is a folder or plain file
 */
ServeFile *sf_new(const char *urlPath, const char *sysPath, int isFolder);


/* Returns isFolder value passed in sf_new()
 */
int sf_isFolder(const ServeFile*);


/* Returns urlPath passed in sf_new()
 */
const char *sf_getUrlPath(const ServeFile*);


/* Returns sysPath passed in sf_new()
 */
const char *sf_getSysPath(const ServeFile*);


/* Returns non-zero when file/directory defined by sysPath can be modified.
 */
int sf_isModifiable(const ServeFile*);


/* Adds a new entry to directory listing.
 */
void sf_addEntry(ServeFile*, const char *name, int isDir, unsigned size);
void sf_addEntryChunk(ServeFile*, const DataChunk *name, int isDir,
        unsigned size);


/* Sorts entries in directory listing. Entries are sorted as follows:
 * folders first, sorted alphabetically, then files, sorted alphabetically.
 */
void sf_sortEntries(ServeFile*);


/* Returns array of entries. The array is terminated with entry having
 * fileName set to NULL.
 */
const FolderEntry *sf_getEntries(const ServeFile*);


/* Ends use of the object.
 */
void sf_free(ServeFile*);


#endif /* SERVEFILE_H */
