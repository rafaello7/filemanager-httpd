#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "folder.h"


/* Parses configuration file.
 */
void config_parse(void);

unsigned config_getListenPort(void);


/* Switches to user specified in configuration file.
 */
int config_switchToTargetUser(void);

/* Returns path in file system corresponding to given path in URL.
 * Returns NULL when the URL path does not have any corresponding
 * system path.
 * The returned value should be released by free().
 */
char *config_getSysPathForUrlPath(const char *urlPath);


Folder *config_getSubSharesForPath(const char *urlPath);


/* Retrieves file to serve for dir. If such file does not exist, returns
 * NULL. NULL is also returned when error occurs. In this case sysErrNo is
 * set to errno. When no error occured, sysErrNo is set to 0.
 */
char *config_getIndexFile(const char *dir, int *sysErrNo);


/* Returns non-zero when directory listing should be done when index.html
 * file does not exist in the directory.
 */
int config_isDirListingAllowed(void);


#endif /* CONFIGFILE_H */
