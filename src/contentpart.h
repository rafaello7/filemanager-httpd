#ifndef CONTENTPART_H
#define CONTENTPART_H

#include "membuf.h"


/* A part of multipart data.
 */
typedef struct ContentPart ContentPart;


/* Creates a new ContentPart object.
 * If the content part contains Content-Disposition header field with
 * "filename" parameter, the content part body will be stored in file
 * under name specified by the "filename" parameter value. The destDir
 * parameter specifies the file location.
 * If the destDir parameter is NULL and "filename" parameter is set,
 * the body is discarded.
 *
 * If the "filename" parameter is not set, the body is stored in memory,
 * regardless of whether the destDir is NULL or not.
 */
ContentPart *cpart_new(const char *destDir);


/* The ContentPart contents build helper.
 * Adds the data arrived as a part of ContentPart.
 */
void cpart_appendData(ContentPart*, const char *data, unsigned len);


/* Returns value of "name" parameter from Content-Type header field.
 * (the control name).
 */
const char *cpart_getName(const ContentPart*);


/* Returns true when the content part name equals to the specifed name.
 */
bool cpart_nameEquals(const ContentPart*, const char *name);


/* Returns value of "filename" parameter from Content-Type header field.
 * Returns NULL when the parameter is not set.
 * When not NULL, the part content is stored in the specified file,
 * in directory provided as destDit at ContentPart object creation.
 */
const char *cpart_getFileName(const ContentPart*);


/* Returns ouptut file full path name. Returns NULL when body is stored in
 * memory or is discarded.
 */
const char *cpart_getFilePathName(const ContentPart*);


/* Returns the part content as string; returns NULL when the content
 * is not stored in memory.
 */
const char *cpart_getDataStr(const ContentPart*);


/* If the body is stored in file and the whole body was stored in output file
 * successfully, makes the output file permanent (i.e. causes that the file
 * will be not deleted at object destruction) and returns true.
 *
 * If the body is not stored in any file, the function returns false.
 * If the body had to be stored in file but the file creation failed,
 * the function stores in sysErrNo the errno value indicating the failure
 * reason.
 */
bool cpart_finishUpload(ContentPart*, int *sysErrNo);


/* Ends use of the object.
 */
void cpart_free(ContentPart*);

#endif /* CONTENTPART_H */
