#include <stdbool.h>
#include "fmconfig.h"
#include "cmdline.h"
#include "datachunk.h"
#include "membuf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>


enum AuthorizationOpt {
    AO_NEVER,
    AO_MODIFY,
    AO_LISTING,
    AO_ALWAYS
};

typedef struct {
    const char *urlpath;
    const char *syspath;
} Share;

static unsigned gListenPort;
static const char *gSwitchUser = "www-data";

/* patterns specified as "index" option in configuration file.
 */
static const char **gIndexPatterns;
static unsigned gIndexPatternCount;


/* non-zero when "index" option contains "."
 */
static bool gIsDirectoryListingAllowed = true;

/* List of shares, terminated with one with urlpath set to NULL
 */
static Share *gShares;

static enum AuthorizationOpt gAuthorizationOpt = AO_NEVER;
static const char **gCredentials;


void config_parse(void)
{
    const char *configFName;
    FILE *fp;
    char buf[1024];
    DataChunk dchName, dchValue, dchPatt;
    int shareCount = 0, lineNo = 0, credentialCount = 0;

    configFName = cmdline_getConfigFileName();
    if( (fp = fopen(configFName, "r")) != NULL ) {
        gIsDirectoryListingAllowed = false;
        while( fgets(buf, sizeof(buf), fp) != NULL ) {
            ++lineNo;
            dch_InitWithStr(&dchValue, buf);
            dch_TrimWS(&dchValue);
            if( dchValue.len == 0 || *dchValue.data == '#' )
                continue;
            if( dch_ExtractTillStrStripWS(&dchValue, &dchName, "=") ) {
                if( dch_StartsWithStr(&dchName, "/") ) {
                    gShares = realloc(gShares,
                            (shareCount+1) * sizeof(Share));
                    dch_TrimTrailing(&dchName, "/");
                    gShares[shareCount].urlpath = dch_DupToStr(&dchName);
                    dch_TrimTrailing(&dchValue, "/");
                    gShares[shareCount].syspath = dch_DupToStr(&dchValue);
                    ++shareCount;
                }else if( dch_EqualsStr(&dchName, "index") ) {
                    while( dch_ExtractTillWS(&dchValue, &dchPatt) ) {
                        if( dch_EqualsStr(&dchPatt, ".") )
                            gIsDirectoryListingAllowed = true;
                        else{
                            gIndexPatterns = realloc(gIndexPatterns,
                                (gIndexPatternCount+1) * sizeof(const char*));
                            gIndexPatterns[gIndexPatternCount++] =
                                dch_DupToStr(&dchPatt);
                        }
                    }
                }else if( dch_EqualsStr(&dchName, "port") ) {
                    if( ! dch_ToUInt(&dchValue, 0, &gListenPort) )
                        fprintf(stderr, "%s:%d warning: unrecognized port",
                                configFName, lineNo);
                }else if( dch_EqualsStr(&dchName, "user") ) {
                    gSwitchUser = dch_DupToStr(&dchValue);
                }else if( dch_EqualsStr(&dchName, "auth") ) {
                    if( dch_EqualsStr(&dchValue, "never") ) {
                        gAuthorizationOpt = AO_NEVER;
                    }else if( dch_EqualsStr(&dchValue, "modify") ) {
                        gAuthorizationOpt = AO_MODIFY;
                    }else if( dch_EqualsStr(&dchValue, "listing") ) {
                        gAuthorizationOpt = AO_LISTING;
                    }else{
                        if( ! dch_EqualsStr(&dchValue, "always") )
                            fprintf(stderr, "%s:%d warning: wrong auth value; "
                                    "assuming \"always\"\n",
                                    configFName, lineNo);
                        gAuthorizationOpt = AO_ALWAYS;
                    }
                }else if( dch_EqualsStr(&dchName, "credentials") ) {
                    gCredentials = realloc(gCredentials,
                            (credentialCount+1) * sizeof(char*));
                    gCredentials[credentialCount] = dch_DupToStr(&dchValue);
                    ++credentialCount;
                }else{
                    fprintf(stderr, "%s:%d warning: unrecognized option "
                            "\"%.*s\", ignored\n", configFName, lineNo,
                            dchName.len, dchName.data);
                }
            }else{
                fprintf(stderr, "%s:%d warning: missing '='; "
                        "line ignored\n", configFName, lineNo);
            }
        }
    }else{
        fprintf(stderr, "WARN: unable to open configuration file %s: %s\n",
                configFName, strerror(errno));
    }
    if( shareCount == 0 ) {
        gShares = realloc(gShares, (shareCount+1) * sizeof(Share));
        gShares[shareCount].urlpath = "";
        gShares[shareCount].syspath = "/var/www/html";
        ++shareCount;
    }
    gShares = realloc(gShares, (shareCount+1) * sizeof(Share));
    gShares[shareCount].urlpath = NULL;
    gShares[shareCount].syspath = NULL;
    if( credentialCount > 0 ) {
        gCredentials = realloc(gCredentials,
                (credentialCount+1) * sizeof(char*));
        gCredentials[credentialCount] = NULL;
    }
    if( gListenPort == 0 ) {
        gListenPort = geteuid() == 0 ? 80 : 8000;
    }
    if( gIndexPatternCount == 0 )
        gIsDirectoryListingAllowed = true;
}

unsigned config_getListenPort(void)
{
    return gListenPort;
}

bool config_switchToTargetUser(void)
{
    int res = true;
    struct passwd *pwd;

    if( gSwitchUser[0] && geteuid() == 0 ) {
        res = false;
        if( (pwd = getpwnam(gSwitchUser)) != NULL ) {
            if( setgid(pwd->pw_gid) != 0 )
                fprintf(stderr, "WARN: setgid: %s\n", strerror(errno));
            if( setuid(pwd->pw_uid) == 0 )
                res = true;
            else
                fprintf(stderr, "setuid: %s\n", strerror(errno));
        }else{
            fprintf(stderr, "No such user \"%s\"; please specify a valid "
                    "switch user in configuration file\n", gSwitchUser);
        }
    }
    return res;
}

char *config_getSysPathForUrlPath(const char *urlPath)
{
    const Share *cur, *best = NULL;
    int urlPathLen, sysPathLen, bestShLen = -1;

    urlPathLen = strlen(urlPath);
    for(cur = gShares; cur->urlpath; ++cur) {
        int shLen = strlen(cur->urlpath);
        if( shLen > urlPathLen || shLen <= bestShLen )
            continue;
        if( !strncmp(urlPath, cur->urlpath, shLen) &&
            (shLen == urlPathLen || urlPath[shLen] == '/') )
        {
            best = cur;
            bestShLen = shLen;
        }
    }
    if( best != NULL ) {
        MemBuf *filePathName = mb_new();
        sysPathLen = strlen(best->syspath);
        mb_appendData(filePathName, best->syspath, sysPathLen);
        if( urlPathLen > bestShLen ) {
            mb_appendData(filePathName, urlPath + bestShLen,
                    urlPathLen - bestShLen);
        }else if( sysPathLen == 0 )
            mb_appendData(filePathName, "/", 1);
        mb_appendData(filePathName, "", 1);
        return mb_unbox_free(filePathName);
    }
    return NULL;
}

Folder *config_getSubSharesForPath(const char *urlPath)
{
    Folder *folder = NULL;
    const Share *cur;
    int pathLen;
    DataChunk dchPath, ent;
    const FolderEntry *fe;

    pathLen = strlen(urlPath);
    while( pathLen > 0 && urlPath[pathLen-1] == '/' )
        --pathLen;
    for(cur = gShares; cur->urlpath; ++cur) {
        if( !strncmp(urlPath, cur->urlpath, pathLen) &&
                cur->urlpath[pathLen] == '/')
        {
            dch_InitWithStr(&dchPath, cur->urlpath + pathLen + 1);
            dch_SkipLeading(&dchPath, "/");
            if( dchPath.len > 0 ) {
                dch_ExtractTillStr(&dchPath, &ent, "/");
                if( folder == NULL )
                    folder = folder_new();
                /* avoid duplicates */
                for(fe = folder_getEntries(folder); fe->fileName != NULL &&
                        !dch_EqualsStr(&ent, fe->fileName); ++fe)
                    ;
                if( fe->fileName == NULL )
                    folder_addEntryChunk(folder, &ent, 1, 0);
            }
        }
    }
    return folder;
}

char *config_getIndexFile(const char *dir, int *sysErrNo)
{
    DIR *d;
    struct dirent *dp;
    struct stat st;
    unsigned dirNameLen, matchIdx, bestMatchIdx = gIndexPatternCount;
    MemBuf *bestIdxFile = mb_new();

    if( (d = opendir(dir)) != NULL ) {
        MemBuf *filePathName = mb_new();
        mb_appendStr(filePathName, dir);
        if( dir[mb_dataLen(filePathName)-1] != '/' )
            mb_appendStr(filePathName, "/");
        dirNameLen = mb_dataLen(filePathName);
        while( bestMatchIdx > 0 && (dp = readdir(d)) != NULL ) {
            if( ! strcmp(dp->d_name, ".") || ! strcmp(dp->d_name, "..") )
                continue;
            for( matchIdx = 0; matchIdx < bestMatchIdx; ++matchIdx ) {
                if( fnmatch(gIndexPatterns[matchIdx], dp->d_name,
                            FNM_PATHNAME | FNM_PERIOD) == 0 )
                    break;
            }
            if( matchIdx < bestMatchIdx ) {
                mb_setDataExtend(filePathName, dirNameLen, dp->d_name,
                        strlen(dp->d_name) + 1);
                if( stat(mb_data(filePathName), &st) == 0 &&
                        ! S_ISDIR(st.st_mode) )
                {
                    mb_setDataExtend(bestIdxFile, 0, mb_data(filePathName),
                            mb_dataLen(filePathName));
                }
            }
        }
        closedir(d);
        mb_free(filePathName);
        *sysErrNo = 0;
    }else{
        *sysErrNo = errno;
    }
    return mb_unbox_free(bestIdxFile);
}

static char *base64Decode(const char *s)
{
    static const unsigned char base64chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static int base64values[128];
    unsigned i, b = 0, bits = 0;
    unsigned char c;
    char *res = malloc(strlen(s) / 4 * 3 + 3), *dest = res;

    if( base64values['+'] == 0 ) {
        for(i = 0; i < 128; ++i)
            base64values[i] = -1;
        for(i = 0; base64chars[i]; ++i)
            base64values[base64chars[i]] = i;
    }
    while( (c = *s++) && c != '=' ) {
        if( c < 128 && base64values[c] != -1 ) {
            b = (b << 6) + base64values[c];
            bits += 6;
            if( bits >= 8 ) {
                bits -= 8;
                *dest++ = b >> bits & 0xff;
            }
        }
    }
    *dest = '\0';
    return res;
}

bool config_isClientAuthorized(const char *authorization)
{
    char *authDec;
    const char **cred;
    bool res;

    if( gCredentials == NULL )
        res = true;
    else if( authorization == NULL || strncasecmp(authorization, "Basic ", 6) )
        res = false;
    else{
        authDec = base64Decode(authorization+6);
        for(cred = gCredentials; *cred != NULL && strcmp(*cred, authDec);
                ++cred)
        {
        }
        free(authDec);
        res = *cred != NULL;
    }
    return res;
}

bool config_isActionAllowed(enum PrivilegedAction pa, bool isLoggedIn)
{
    if( pa != PA_SERVE_PAGE && ! gIsDirectoryListingAllowed )
        return false;
    if( isLoggedIn || gAuthorizationOpt == AO_NEVER )
        return true;
    switch( pa ) {
    case PA_SERVE_PAGE:
        return gAuthorizationOpt == AO_LISTING ||
            gAuthorizationOpt == AO_MODIFY;
    case PA_LIST_FOLDER:
        return gAuthorizationOpt == AO_MODIFY;
    default:    /* PA_MODIFY */
        break;
    }
    return false;
}

bool config_givesLoginMorePrivileges(void)
{
    return gAuthorizationOpt != AO_NEVER;
}

