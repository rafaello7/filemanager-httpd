#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "servefile.h"


/* Parses configuration file.
 */
void config_parse(void);

unsigned config_getListenPort(void);


/* Returns path in file system corresponding to given path in URL.
 * Returns NULL when the URL path does not have any corresponding
 * system path.
 * The returned value should be released by free().
 */
char *config_getSysPathForUrlPath(const char *urlPath);


/* Returns file to serve for given URL path.
 */
ServeFile *config_getServeFile(const char *urlPath, int *sysErrNum);


#endif /* CONFIGFILE_H */
