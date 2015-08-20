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
 * When not NULL, the part content is stored in a file, in directory
 * provided as destDir at ContentPart object creation.
 * If the destDir already contains a file with this name, a random name is
 * chosen for output file. Otherwise the output is stored in file
 * with this name.
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
 * successfully, renames the file to targetName, makes the output file
 * permanent (i.e. causes that file will be not deleted at object
 * destruction) and returns true.
 *
 * The targetName may be relative or absolute. Relative name is related to
 * destDir. The targetName may be NULL. In this case the file is stored
 * with name returned by cpart_getFileName(). If a file with the target name
 * already exists, the result depends on replaceIfExists parameter value.
 * If false, the function fails with sysErrNo set to EEXIST.
 *
 * If the body is not stored in any file, the function returns false.
 * If the body had to be stored in a file but the file creation failed,
 * the function stores in sysErrNo the errno value indicating the failure
 * reason.
 */
bool cpart_finishUpload(ContentPart*, const char *targetName,
        bool replaceIfExists, int *sysErrNo);


/* Ends use of the object.
 */
void cpart_free(ContentPart*);

#endif /* CONTENTPART_H */
