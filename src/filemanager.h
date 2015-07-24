#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "requestbuf.h"
#include "respbuf.h"


/* Produces response for the specified HTTP request.
 */
RespBuf *filemgr_processRequest(const RequestBuf*);

#endif /* FILEMANAGER_H */
