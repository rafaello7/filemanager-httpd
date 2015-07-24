#ifndef REQUESTBUF_H
#define REQUESTBUF_H

#include "membuf.h"

typedef struct RequestBuf RequestBuf;


RequestBuf *req_new(void);

/* Request build helper. Appends new data which arrived from connection
 * as a part of request.
 * Returns -1 when the request is not complete. Otherwise
 * (value >= 0) - number of bytes consumed from data.
 */
int req_appendData(RequestBuf*, const char *data, unsigned len);


const char *req_getMethod(const RequestBuf*);
const char *req_getPath(const RequestBuf*);
const char *req_getHeaderVal(const RequestBuf*, const char *headerName);
const MemBuf *req_getBody(const RequestBuf*);


void req_free(RequestBuf*);


#endif /* REQUESTBUF_H */
