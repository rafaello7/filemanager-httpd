#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "requestheader.h"
#include "respbuf.h"


typedef struct FileManager FileManager;

enum PostingResult {
    PR_PROCESSED,
    PR_REQUIRE_AUTH
};


FileManager *filemgr_new(const char *sysPath, const RequestHeader*);


void filemgr_consumeBodyBytes(FileManager*, const char *data, unsigned len);


/* Processes POST request on folder
 */
enum PostingResult filemgr_processPost(FileManager*, const RequestHeader*);


/* Returns response page containing folder contents.
 */
RespBuf *filemgr_printFolderContents(const FileManager*,
        const RequestHeader*, int *sysErrNo);


void filemgr_free(FileManager*);

/* Returns a <form> with login button.
 */
const char *filemgr_getLoginForm(void);


#endif /* FILEMANAGER_H */
