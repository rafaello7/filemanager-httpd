#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "requestheader.h"
#include "respbuf.h"


enum PostingResult {
    PR_PROCESSED,
    PR_REQUIRE_AUTH
};

/* Processes POST request on folder
 */
enum PostingResult filemgr_processPost(const RequestHeader*,
        const MemBuf *body, const char *sysPath, char **errMsgBuf);


/* Returns response page containing folder contents.
 */
RespBuf *filemgr_printFolderContents(const char *urlPath, const Folder *folder,
        bool isModifiable, bool showLoginButton, const char *errorMsg,
        bool onlyHead);


/* Returns a <form> with login button.
 */
const char *filemgr_getLoginForm(void);


#endif /* FILEMANAGER_H */
