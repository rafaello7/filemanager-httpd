#ifndef REQUESTBUF_H
#define REQUESTBUF_H

#include "membuf.h"


/* HTTP request buffer
 */
typedef struct RequestBuf RequestBuf;


/* Creates a new request.
 */
RequestBuf *req_new(void);


/* Request build helper. Appends new data which arrived from connection
 * as a part of request.
 * Returns -1 when the request is not complete. Otherwise
 * (value >= 0) - number of bytes consumed from data.
 */
int req_appendData(RequestBuf*, const char *data, unsigned len);


/* Returns the request method (GET, POST, etc.)
 */
const char *req_getMethod(const RequestBuf*);


/* Returns path specified in request line.
 * The returned path is already decoded.
 */
const char *req_getPath(const RequestBuf*);


/* Returns value of the specified header; returns NULL header with the
 * specified name does not exist.
 */
const char *req_getHeaderVal(const RequestBuf*, const char *headerName);


/* Returns the request body.
 */
const MemBuf *req_getBody(const RequestBuf*);


/* Ends use of the request.
 */
void req_free(RequestBuf*);


#endif /* REQUESTBUF_H */
