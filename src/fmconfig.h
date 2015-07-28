#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "servefile.h"


/* Parses configuration file.
 */
void config_parse(void);

unsigned config_getListenPort(void);


char *config_getSysPathForUrlPath(const char *urlPath);

/* Returns file to serve.
 */
ServeFile *config_getServeFile(const char *urlPath);


#endif /* CONFIGFILE_H */
