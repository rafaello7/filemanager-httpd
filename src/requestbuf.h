#ifndef REQUESTBUF_H
#define REQUESTBUF_H

#include "membuf.h"
#include "fmconfig.h"
#include "requestheader.h"

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


const RequestHeader *req_getHeader(const RequestBuf*);


/* Returns the request body.
 */
const MemBuf *req_getBody(const RequestBuf*);


/* Ends use of the request.
 */
void req_free(RequestBuf*);


#endif /* REQUESTBUF_H */
