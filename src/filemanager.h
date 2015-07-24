#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "requestbuf.h"
#include "respbuf.h"

RespBuf *filemgr_processRequest(const RequestBuf*);

#endif /* FILEMANAGER_H */
