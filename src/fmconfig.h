#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "folder.h"


enum PrivilegedAction {
    PA_SERVE_PAGE,
    PA_LIST_FOLDER,
    PA_MODIFY,
};

/* Parses configuration file.
 */
void config_parse(void);

unsigned config_getListenPort(void);


/* Switches to user specified in configuration file.
 */
bool config_switchToTargetUser(void);

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


/* Parameter is the "Authorization" header field value.
 * Returns true when the client authorization has passed, false otherwise.
 */
bool config_isClientAuthorized(const char *authorization);


/* Returns true when the privileged action is allowed.
 */
bool config_isActionAllowed(enum PrivilegedAction, bool isLoggedIn);


/* Returns true when more privileged actions are allowed by
 * logged in user than not logged in.
 */
bool config_givesLoginMorePrivileges(void);

#endif /* CONFIGFILE_H */
