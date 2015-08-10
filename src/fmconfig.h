#ifndef FMCONFIG_H
#define FMCONFIG_H

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


/* Returns true when the specified path does match some CGI pattern.
 */
bool config_isCGI(const char *urlPath);


/* Stores in md5sum a MD5 sum of string constructed as concatenation of:
 *      username ":" realm ":" password
 * Returns true on success, false when credentials for the user don't exist.
 * The userNameLen is length of userName string or may be -1 when the string
 * is terminated with '\0'.
 *
 * The string is formed per definition of A1 value in Digest authorization.
 * See RFC 2617 (Basic and Digest Access Authentication), section 3.2.2.2
 */
bool config_getDigestAuthCredential(const char *userName, int userNameLen,
        char *md5sum);


/* Returns encoded "credential" value to use in configuration file.
 */
const char *config_getCredentialsEncoded(const char *userWithPasswd);


/* Returns true when the privileged action is set as "supported" in config.
 */
bool config_isActionAvailable(enum PrivilegedAction);


/* Returns true when the privileged action is allowed.
 */
bool config_isActionAllowed(enum PrivilegedAction, bool isLoggedIn);


/* Returns true when more privileged actions are allowed by
 * logged in user than not logged in.
 */
bool config_givesLoginMorePrivileges(void);

#endif /* FMCONFIG_H */
