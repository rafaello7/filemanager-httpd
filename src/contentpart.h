#ifndef CONTENTPART_H
#define CONTENTPART_H

#include "membuf.h"


/* A part of multipart data.
 */
typedef struct ContentPart ContentPart;


ContentPart *cpart_new(const char *destDir);


void cpart_appendData(ContentPart*, const char *data, unsigned len);


/* Returns value of "name" parameter from Content-Type header field.
 * (the control name).
 */
const char *cpart_getName(const ContentPart*);


/* Returns true when the content part name equals to the specifed name.
 */
bool cpart_nameEquals(const ContentPart*, const char *name);


/* Returns value of "filename" parameter from Content-Type header field.
 * Returns NULL when the parameter does not exist.
 * When not NULL, the part content is stored in the specified file,
 * in directory provided at ContentPart object creation.
 */
const char *cpart_getFileName(const ContentPart*);


/* Returns the part content; returns NULL when the content is stored in file.
 */
const char *cpart_getDataStr(const ContentPart*);


bool cpart_finishUpload(ContentPart*, int *sysErrNo);


/* Ends use of the object.
 * If the part content was stored in file, the keepFile, it is retained
 * when keepFile is set to true. Otherwise it is removed.
 */
void cpart_free(ContentPart*, bool keepFile);

#endif /* CONTENTPART_H */
