#ifndef RESPBUF_H
#define RESPBUF_H


#include "datachunk.h"
#include "datareadyselector.h"
#include "responsesender.h"


/* HTTP response buffer.
 */
typedef struct RespBuf RespBuf;

typedef enum {
    HTTP_200_OK                     = 200,
    HTTP_403_FORBIDDEN              = 403,
    HTTP_404_NOT_FOUND              = 404,
    HTTP_500                        = 500   /* Internal server error */
} HttpStatus;


const char *resp_cmnStatus(HttpStatus);


/* Creates a new response.
 */
RespBuf *resp_new(const char *status, bool onlyHead);


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


/* Appends data/string with replaced special HTML characters by entities.
 */
void resp_appendDataEscapeHtml(RespBuf*, const char *data, unsigned len);
void resp_appendStrEscapeHtml(RespBuf*, const char *str);
void resp_appendChunkEscapeHtml(RespBuf*, const DataChunk*);


/* Finishes response preparation. Free the buffer, return data ready to send.
 */
ResponseSender *resp_finish(RespBuf*);


#endif /* RESPBUF_H */
