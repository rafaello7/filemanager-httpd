#ifndef RESPBUF_H
#define RESPBUF_H

#include "datasource.h"


/* HTTP response buffer.
 */
typedef struct RespBuf RespBuf;

typedef enum {
    HTTP_200_OK                     = 200,
    HTTP_301_MOVED_PERMANENTLY      = 301,
    HTTP_401_UNAUTHORIZED           = 401,
    HTTP_403_FORBIDDEN              = 403,
    HTTP_404_NOT_FOUND              = 404,
    HTTP_405_METHOD_NOT_ALLOWED     = 405,
    HTTP_500                        = 500   /* Internal server error */
} HttpStatus;

/* Creates a new response.
 * Parameter sysErrno may be:
 *  >= 0    - errno value indicating the error
 *  < 0     - negated HTTP error, e.g. -405 for HTTP error 405
 */
RespBuf *resp_new(HttpStatus);


/* Returns error message associated with sysErrno given in resp_new().
 * Returns NULL when sysErrno was 0.
 */
const char *resp_getErrorMessage(RespBuf*);


/* Adds header to response
 */
void resp_appendHeader(RespBuf*, const char *name, const char *value);


/* Appends data to response body
 */
void resp_appendData(RespBuf*, const char *data, unsigned dataLen);


/* Sets file descriptor as source of bytes in response body. Previously set
 * file descriptor is closed.
 * The descriptor should be opened for read. File contents is not modified.
 * The function takes ownership over the file descriptor (closes after use).
 * The file contents is used as data source after exhaustion of data set by
 * resp_appendData().
 */
void resp_enqFile(RespBuf*, int fileDescriptor);


/* Appends string to response body, i.e. strlen(str) bytes.
 */
void resp_appendStr(RespBuf*, const char *str);


/* Appends list of strings to response body.
 * The list shall be terminated with NULL.
 */
void resp_appendStrL(RespBuf*, const char *str1, const char *str2,  ...);


/* Ends use of the response. Returns the response raw bytes to send.
 */
DataSource *resp_finish(RespBuf*, bool onlyHead);


#endif /* RESPBUF_H */
