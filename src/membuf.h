#ifndef MEMBUF_H
#define MEMBUF_H

#include "datachunk.h"


/* A memory buffer - hyper advanced memory management.
 */
typedef struct MemBuf MemBuf;


/* Creates a new buffer.
 */
MemBuf *mb_new(void);


/* Appends data to buffer
 */
void mb_appendData(MemBuf*, const char*, unsigned);


/* Appends another buffer
 */
void mb_appendBuf(MemBuf*, const MemBuf*);


/* Appends string (terminated with '\0').
 * The zero byte terminating string is NOT appended.
 */
void mb_appendStr(MemBuf*, const char*);


void mb_appendStrL(MemBuf*, const char *str1, const char *str2, ...);


void mb_appendChunk(MemBuf*, const DataChunk*);


bool mb_endsWithStr(const MemBuf*, const char*);


/* Resizes the buffer. Current buffer contents is preserved.
 * New data is uninitialized.
 */
void mb_resize(MemBuf*, unsigned newSize);


/* Returns the buffer contents.
 * Contents length may be obtained using mb_datalen.
 */
const char *mb_data(const MemBuf*);


/* Copies data chunk into buffer.
 * It is an error when the parameters refer beyond buffer end.
 */
void mb_setData(MemBuf*, unsigned offset, const char *data, unsigned len);


/* Copies data chunk into buffer.
 * The buffer size is extended when needed.
 */
void mb_setDataExtend(MemBuf*, unsigned offset, const char *data, unsigned len);


/* Copies the specified string with terminating '\0' character into buffer
 * starting at the specified offset. The buffer is truncated after the '\0'
 * character.
 * Offset shall be less than or equal to buffer length.
 */
void mb_setStrZEnd(MemBuf*, unsigned offset, const char *str);


/* Returns length of buffer
 */
unsigned mb_dataLen(const MemBuf*);


/* Ends use of MemBuf.
 */
void mb_free(MemBuf*);


/* Ends use of MemBuf. Returns the allocated internal buffer.
 */
char *mb_unbox_free(MemBuf*);


#endif /* MEMBUF_H */
