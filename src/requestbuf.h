#ifndef REQUESTBUF_H
#define REQUESTBUF_H

#include "membuf.h"
#include "fmconfig.h"

enum LoginState {
    LS_LOGGED_OUT,      /* request does not contain "Authorization" header */
    LS_LOGGED_IN,       /* request contains valid "Authorization" header */
    LS_LOGIN_FAIL       /* request contains invalid "Authorization" header */
};

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


enum LoginState req_getLoginState(const RequestBuf*);


/* Returns true when client might be interested with log in, i.e.:
 *  1. Is not logged in yet
 *  2. Some additional actions will be possible after login
 */
bool req_isWorthPuttingLogOnButton(const RequestBuf*);


/* Returns true when user has privileges to perform given action.
 */
bool req_isActionAllowed(const RequestBuf*, enum PrivilegedAction);


/* Returns the request body.
 */
const MemBuf *req_getBody(const RequestBuf*);


/* Ends use of the request.
 */
void req_free(RequestBuf*);


#endif /* REQUESTBUF_H */
