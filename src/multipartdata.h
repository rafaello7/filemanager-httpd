#ifndef MULTIPARTDATA_H
#define MULTIPARTDATA_H

#include "contentpart.h"


typedef struct MultipartData MultipartData;


/* Creates new MultipartData object.
 * Parameters:
 *   boundaryDelimiter  - "boundary" value from Content-Type header
 *   destDir            - output files location for content parts having
 *                        "filename" parameter in Content-Disposition header.
 */
MultipartData *mpdata_new(const char *boundaryDelimiter, const char *destDir);


/* MIME build helper. Adds the data arrived as the part of multipart content.
 */
void mpdata_appendData(MultipartData*, const char *data, unsigned len);


/* Returns the given part of multipart content.
 * Returns NULL when the partNum exceeds number of content parts.
 */
const ContentPart *mpdata_getPart(MultipartData*, unsigned partNum);


/* Returns content part having the given "name" parameter value in
 * Content-Disposition header field.
 * Returns NULL when such part does not exist.
 */
ContentPart *mpdata_getPartByName(MultipartData*, const char *name);


/* Returns true when there is a part having the given "name" parameter value
 * in Content-Disposition header field.
 */
bool mpdata_containsPartWithName(MultipartData*, const char *name);


/* Ends use of MultipartData
 */
void mpdata_free(MultipartData*);

#endif /* MULTIPARTDATA_H */
