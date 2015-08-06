#ifndef MEMBUF_H
#define MEMBUF_H

#include "datachunk.h"


/* A memory buffer - hyper advanced memory management.
 */
typedef struct MemBuf MemBuf;


/* Creates a new buffer.
 */
MemBuf *mb_new(void);


/* If pointer buffer value is NULL, creates a new MemBuf and stores in the
 * buffer. Otherwise does nothing.
 */
void mb_newIfNull(MemBuf**);


/* Creates a new buffer initialized with the specified string contents.
 * The function call:
 *      MemBuf *mb = mb_newWithStr(aString);
 * is equivalent of:
 *      MemBuf *mb = mb_new()
 *      mb_appendStr(aString);
 */
MemBuf *mb_newWithStr(const char*);


/* Appends data to buffer.
 */
void mb_appendData(MemBuf*, const char*, unsigned);


/* Appends string (strlen() characters).
 */
void mb_appendStr(MemBuf*, const char*);


/* Appends list of string parameters. The list shall be terminated with NULL.
 */
void mb_appendStrL(MemBuf*, const char *str1, const char *str2, ...);


void mb_appendChunk(MemBuf*, const DataChunk*);


bool mb_endsWithStr(const MemBuf*, const char*);


/* Resizes the buffer. Current buffer contents is preserved.
 * New data is uninitialized.
 */
void mb_resize(MemBuf*, unsigned newSize);


/* Returns the buffer contents.
 * Contents length may be obtained using mb_dataLen.
 * The data contains extra byte and end, with value '\0'.
 */
const char *mb_data(const MemBuf*);


/* Returns length of buffer
 */
unsigned mb_dataLen(const MemBuf*);


/* Copies data chunk into buffer.
 * It is an error when the parameters refer beyond buffer end.
 */
void mb_setData(MemBuf*, unsigned offset, const char *data, unsigned len);


/* Fills buffer using read().
 */
int mb_readFile(MemBuf*, int fd, unsigned bufOffset, unsigned toRead);


/* Fills buffer with '\0' bytes.
 */
void mb_fillWithZeros(MemBuf*, unsigned offset, unsigned len);


/* Copies the specified string into buffer starting at the specified offset.
 * The buffer is truncated at the string end.
 * Offset shall be less than or equal to buffer length.
 */
void mb_setStrEnd(MemBuf*, unsigned offset, const char *str);


/* Ends use of MemBuf.
 */
void mb_free(MemBuf*);


/* Ends use of MemBuf.
 * If the parameter value is NULL, the function returns NULL.
 * Otherwise returns the allocated internal buffer of MemBuf.
 * The allocated buffer is 1 byte longer than length indicated by mb_dataLen().
 * The extra byte contains '\0'.
 */
char *mb_unbox_free(MemBuf*);


#endif /* MEMBUF_H */
