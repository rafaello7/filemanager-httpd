#ifndef REQUESTHEADER_H
#define REQUESTHEADER_H

#include "fmconfig.h"


/* HTTP request header
 */
typedef struct RequestHeader RequestHeader;


enum LoginState {
    LS_LOGGED_OUT,      /* request does not contain "Authorization" header */
    LS_LOGGED_IN,       /* request contains valid "Authorization" header */
    LS_LOGIN_FAIL       /* request contains invalid "Authorization" header */
};


/* Creates a new request.
 */
RequestHeader *reqhdr_new(void);


/* Request header build helper. Appends new data which arrived from connection
 * as a part of request header.
 * Returns -1 when the request header is not complete. Otherwise
 * (value >= 0) - number of bytes consumed from data.
 */
int reqhdr_appendData(RequestHeader*, const char *data, unsigned len);


/* Returns the request method (GET, POST, etc.)
 */
const char *reqhdr_getMethod(const RequestHeader*);


/* Returns path specified in request line.
 * The returned path is already decoded.
 */
const char *reqhdr_getPath(const RequestHeader*);


/* If the idx does exceed the size of header array, returns false.
 * Otherwise stores in nameBuf and valueBuf the header name and value
 * and returns true.
 */
bool reqhdr_getHeaderAt(const RequestHeader*, unsigned idx,
        const char **nameBuf, const char **valueBuf);


/* Returns value of the specified header; returns NULL when header with the
 * specified name does not exist.
 */
const char *reqhdr_getHeaderVal(const RequestHeader*, const char *headerName);


enum LoginState reqhdr_getLoginState(const RequestHeader*);


/* Returns true when client might be interested with log in, i.e.:
 *  1. Is not logged in yet
 *  2. Some additional actions will be possible after login
 */
bool reqhdr_isWorthPuttingLogOnButton(const RequestHeader*);


/* Returns true when user has privileges to perform given action.
 */
bool reqhdr_isActionAllowed(const RequestHeader*, enum PrivilegedAction);


/* Ends use of the request header.
 */
void reqhdr_free(RequestHeader*);


#endif /* REQUESTHEADER_H */
