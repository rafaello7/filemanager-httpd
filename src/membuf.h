#ifndef MEMBUF_H
#define MEMBUF_H

/* Memory buffer - hyper advanced memory management.
 */

typedef struct MemBuf MemBuf;

/* Creates new buffer.
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

/* Resizes the buffer. Current buffer contents is preserved.
 * New data is uninitialized.
 */
void mb_resize(MemBuf*, unsigned newSize);

/* Returns the buffer contents.
 * Contents length may be obtained using mb_datalen.
 */
const void *mb_data(const MemBuf*);

/* Copies data chunk into buffer.
 */
void mb_setData(MemBuf*, unsigned offset, const char *data, unsigned len);

/* Returns length of buffer
 */
unsigned mb_dataLen(const MemBuf*);

/* Ends use of MemBuf.
 */
void mb_free(MemBuf*);

#endif /* MEMBUF_H */
