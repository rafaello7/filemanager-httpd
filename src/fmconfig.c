#include <stdbool.h>
#include "fmconfig.h"
#include "cmdline.h"
#include "datachunk.h"
#include "membuf.h"
#include "md5calc.h"
#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>


enum DirectoryOps {
    DO_NONE,
    DO_FILE,
    DO_LISTING,
    DO_ALL
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

/* CGI script patterns
 */
static const char **gCgiPatterns;
static unsigned gCgiPatternCount;

/* List of shares, terminated with one with urlpath set to NULL
 */
static Share *gShares;


/* Available operations.
 */
static enum DirectoryOps gAvailOps = DO_ALL;


/* Operations which may be performed by guest.
 */
static enum DirectoryOps gGuestOps = DO_ALL;

static const char **gCredentials;


/* Maximum number of open client connections.
 */
static unsigned gMaxClients = 10;


static void parseFile(const char *configFName, int *shareCount,
        int *credentialCount)
{
    FILE *fp;
    char buf[1024];
    DataChunk dchName, dchValue, dchPatt;
    int lineNo = 0;

    if( (fp = fopen(configFName, "r")) != NULL ) {
        while( fgets(buf, sizeof(buf), fp) != NULL ) {
            ++lineNo;
            dch_initWithStr(&dchValue, buf);
            dch_trimWS(&dchValue);
            if( dchValue.len == 0 || *dchValue.data == '#' )
                continue;
            if( dch_extractTillChrStripWS(&dchValue, &dchName, '=') ) {
                if( dch_startsWithStr(&dchName, "/") ) {
                    gShares = realloc(gShares,
                            (*shareCount+1) * sizeof(Share));
                    dch_trimTrailing(&dchName, '/');
                    gShares[*shareCount].urlpath = dch_dupToStr(&dchName);
                    dch_trimTrailing(&dchValue, '/');
                    gShares[*shareCount].syspath = dch_dupToStr(&dchValue);
                    ++*shareCount;
                }else if( dch_equalsStr(&dchName, "index") ) {
                    int countPre = gIndexPatternCount;
                    while( dch_extractTillWS(&dchValue, &dchPatt) ) {
                        gIndexPatterns = realloc(gIndexPatterns,
                            (gIndexPatternCount+1) * sizeof(const char*));
                        gIndexPatterns[gIndexPatternCount++] =
                            dch_dupToStr(&dchPatt);
                    }
                    if( gIndexPatternCount == countPre ) {
                        free(gIndexPatterns);
                        gIndexPatterns = NULL;
                        gIndexPatternCount = 0;
                    }
                }else if( dch_equalsStr(&dchName, "cgi") ) {
                    int countPre = gCgiPatternCount;
                    while( dch_extractTillWS(&dchValue, &dchPatt) ) {
                        gCgiPatterns = realloc(gCgiPatterns,
                            (gCgiPatternCount+1) * sizeof(const char*));
                        gCgiPatterns[gCgiPatternCount++] =
                            dch_dupToStr(&dchPatt);
                    }
                    if( gCgiPatternCount == countPre ) {
                        free(gCgiPatterns);
                        gCgiPatterns = NULL;
                        gCgiPatternCount = 0;
                    }
                }else if( dch_equalsStr(&dchName, "port") ) {
                    if( ! dch_toUInt(&dchValue, 0, &gListenPort) )
                        fprintf(stderr, "%s:%d warning: unrecognized port",
                                configFName, lineNo);
                }else if( dch_equalsStr(&dchName, "user") ) {
                    gSwitchUser = dch_dupToStr(&dchValue);
                }else if( dch_equalsStr(&dchName, "dirops") ) {
                    if( dch_equalsStr(&dchValue, "all") ) {
                        gAvailOps = DO_ALL;
                    }else if( dch_equalsStr(&dchValue, "listing") ) {
                        gAvailOps = DO_LISTING;
                    }else{
                        if( ! dch_equalsStr(&dchValue, "none") )
                            fprintf(stderr, "%s:%d warning: bad dirops value; "
                                    "assuming \"none\"\n", configFName, lineNo);
                        gAvailOps = DO_NONE;
                    }
                }else if( dch_equalsStr(&dchName, "guestops") ) {
                    if( dch_equalsStr(&dchValue, "all") ) {
                        gGuestOps = DO_ALL;
                    }else if( dch_equalsStr(&dchValue, "listing") ) {
                        gGuestOps = DO_LISTING;
                    }else if( dch_equalsStr(&dchValue, "file") ) {
                        gGuestOps = DO_FILE;
                    }else{
                        if( ! dch_equalsStr(&dchValue, "none") )
                            fprintf(stderr, "%s:%d warning: bad guestops "
                                    "value; assuming \"none\"\n",
                                    configFName, lineNo);
                        gGuestOps = DO_NONE;
                    }
                }else if( dch_equalsStr(&dchName, "credentials") ) {
                    gCredentials = realloc(gCredentials,
                            (*credentialCount+1) * sizeof(char*));
                    gCredentials[*credentialCount] = dch_dupToStr(&dchValue);
                    ++*credentialCount;
                }else if( dch_equalsStr(&dchName, "maxclients") ) {
                    if( ! dch_toUInt(&dchValue, 0, &gMaxClients) )
                        fprintf(stderr, "%s:%d warning: unrecognized "
                                "maxclients value", configFName, lineNo);
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
        fclose(fp);
    }else{
        fprintf(stderr, "WARN: unable to read configuration from %s: %s\n",
                configFName, strerror(errno));
    }
}

void config_parse(void)
{
    int shareCount = 0, credentialCount = 0, sysErrNo, len, dirNameLen;
    const char *configLoc = cmdline_getConfigLoc();
    Folder *folder;
    const FolderEntry *fe;

    if( (folder = folder_loadDir(configLoc, &sysErrNo)) != NULL ) {
        MemBuf *filePathName = mb_newWithStr(configLoc);
        if( ! mb_endsWithStr(filePathName, "/") )
            mb_appendStr(filePathName, "/");
        dirNameLen = mb_dataLen(filePathName);
        for(fe = folder_getEntries(folder); fe->fileName != NULL; ++fe) {
            if( fe->isDir )
                continue;
            len = strlen(fe->fileName);
            if( len >= 5 && !strcmp(fe->fileName + len - 5, ".conf") ) {
                mb_setStrEnd(filePathName, dirNameLen, fe->fileName);
                parseFile(mb_data(filePathName), &shareCount, &credentialCount);
            }
        }
        mb_free(filePathName);
    }else
        parseFile(configLoc, &shareCount, &credentialCount);
    if( shareCount == 0 ) {
        gShares = realloc(gShares, (shareCount+1) * sizeof(Share));
        gShares[shareCount].urlpath = "";
        gShares[shareCount].syspath = HTMLDIR "/welcome.html";
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
    int urlPathLen, bestShLen = -1;

    if( !strcmp(urlPath, "/" ) )
        urlPath = "";
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
        MemBuf *filePathName = mb_newWithStr(best->syspath);
        if( urlPathLen > bestShLen ) {
            mb_appendData(filePathName, urlPath + bestShLen,
                    urlPathLen - bestShLen);
        }else if( mb_dataLen(filePathName) == 0 )
            mb_appendStr(filePathName, "/");
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
    struct stat st;

    pathLen = strlen(urlPath);
    while( pathLen > 0 && urlPath[pathLen-1] == '/' )
        --pathLen;
    for(cur = gShares; cur->urlpath; ++cur) {
        if( !strncmp(urlPath, cur->urlpath, pathLen) &&
                cur->urlpath[pathLen] == '/')
        {
            dch_initWithStr(&dchPath, cur->urlpath + pathLen + 1);
            dch_skipLeading(&dchPath, '/');
            if( dchPath.len > 0 ) {
                dch_extractTillChr(&dchPath, &ent, '/');
                if( folder == NULL )
                    folder = folder_new();
                /* avoid duplicates */
                for(fe = folder_getEntries(folder); fe->fileName != NULL &&
                        !dch_equalsStr(&ent, fe->fileName); ++fe)
                    ;
                if( fe->fileName == NULL ) {
                    if( dchPath.len || stat(cur->syspath, &st) < 0 )
                        folder_addEntryChunk(folder, &ent, true, 
                                S_IRWXU|S_IRWXG|S_IRWXO, 0);
                    else
                        folder_addEntryChunk(folder, &ent,
                                S_ISDIR(st.st_mode), st.st_mode, st.st_size);
                }
            }
        }
    }
    if( folder != NULL )
        folder_sortEntries(folder);
    return folder;
}

char *config_getIndexFile(const char *dir, int *sysErrNo)
{
    DIR *d;
    struct dirent *dp;
    struct stat st;
    unsigned dirNameLen, matchIdx, bestMatchIdx = gIndexPatternCount;
    MemBuf *bestIdxFile = NULL;

    if( (d = opendir(dir)) != NULL ) {
        MemBuf *filePathName = mb_newWithStr(dir);
        if( ! mb_endsWithStr(filePathName, "/") )
            mb_appendStr(filePathName, "/");
        dirNameLen = mb_dataLen(filePathName);
        while( bestMatchIdx > 0 && (dp = readdir(d)) != NULL ) {
            if( ! strcmp(dp->d_name, ".") || ! strcmp(dp->d_name, "..") )
                continue;
            for( matchIdx = 0; matchIdx < bestMatchIdx; ++matchIdx ) {
                if( fnmatch(gIndexPatterns[matchIdx], dp->d_name,
                            FNM_PERIOD) == 0 )
                    break;
            }
            if( matchIdx < bestMatchIdx ) {
                mb_setStrEnd(filePathName, dirNameLen, dp->d_name);
                if( stat(mb_data(filePathName), &st) == 0 &&
                        ! S_ISDIR(st.st_mode) )
                {
                    mb_newIfNull(&bestIdxFile);
                    mb_setStrEnd(bestIdxFile, 0, mb_data(filePathName));
                    bestMatchIdx = matchIdx;
                }
            }
        }
        closedir(d);
        mb_free(filePathName);
        *sysErrNo = 0;
    }else{
        *sysErrNo = errno;
    }
    return bestIdxFile ? mb_unbox_free(bestIdxFile) : NULL;
}

bool config_isCGI(const char *urlPath)
{
    unsigned i;
    const char *cgiPatt, *subPath;
    bool match = false;

    for( i = 0; i < gCgiPatternCount && !match; ++i) {
        cgiPatt = gCgiPatterns[i];
        if( cgiPatt[0] == '/' )
            match = fnmatch(cgiPatt, urlPath, FNM_PATHNAME|FNM_PERIOD) == 0;
        else{
            subPath = urlPath;
            while( subPath != NULL && ! match ) {
                ++subPath;
                match = fnmatch(cgiPatt, subPath, FNM_PATHNAME|FNM_PERIOD) == 0;
                subPath = strchr(subPath, '/');
            }
        }
    }
    return match;
}

bool config_findCGI(const char *urlPath, char **cgiExeBuf, char **cgiUrlBuf,
        char **cgiSubPathBuf)
{
    char *cgiExe = NULL, *cgiUrl = NULL, *cgiSubPath = NULL, *slashPos;
    struct stat st;
    bool isCGI;

    cgiUrl = strdup(urlPath);
    isCGI = config_isCGI(cgiUrl) &&
            (cgiExe = config_getSysPathForUrlPath(cgiUrl)) != NULL &&
            stat(cgiExe, &st) == 0 && S_ISREG(st.st_mode);
    if( ! isCGI ) {
        free(cgiExe);
        slashPos = strrchr(cgiUrl, '/');
        while( ! isCGI && slashPos != NULL ) {
            *slashPos = '\0';
            if( config_isCGI(cgiUrl[0] ? cgiUrl : "/") ) {
                cgiExe = config_getSysPathForUrlPath(cgiUrl);
                if( cgiExe != NULL && stat(cgiExe, &st) == 0 &&
                        S_ISREG(st.st_mode) )
                {
                    isCGI = true;
                    cgiSubPath = slashPos;
                }else
                    free(cgiExe);
            }
            *slashPos = '/';
            while( (slashPos = slashPos == cgiUrl ? NULL : slashPos - 1) &&
                    *slashPos != '/')
                ;
        }
    }
    if( isCGI ) {
        *cgiExeBuf = cgiExe;
        *cgiUrlBuf = cgiUrl;
        if( cgiSubPath != NULL ) {
            slashPos = cgiSubPath;
            cgiSubPath = strdup(cgiSubPath);
            *slashPos = '\0';
        }
        *cgiSubPathBuf = cgiSubPath;
    }else{
        free(cgiUrl);
    }
    return cgiSubPath != NULL;
}

bool config_getDigestAuthCredential(const char *userName, int userNameLen,
        char *md5sum)
{
    const char **cred, *passwdBeg, *found = NULL;
    bool res = false;

    if( userNameLen == -1 )
        userNameLen = strlen(userName);
    if( gCredentials != NULL ) {
        for(cred = gCredentials; *cred != NULL && found == NULL; ++cred) {
            if( ! strncmp(*cred, userName, userNameLen) ) {
                passwdBeg = *cred + userNameLen;
                if( *passwdBeg == ':' || (strlen(passwdBeg) == 32 &&
                        strchr(passwdBeg, ':') == NULL) )
                    found = passwdBeg;
            }
        }
        if( found != NULL ) {
            if( *found == ':' ) {
                MemBuf *mb = mb_new();
                mb_appendData(mb, *cred, userNameLen + 1);
                mb_appendStr(mb, FM_REALM);
                mb_appendStr(mb, *cred + userNameLen);
                md5_calculate(md5sum, mb_data(mb), mb_dataLen(mb));
                mb_free(mb);
            }else   /* already encoded */
                strcpy(md5sum, found);
            res = true;
        }
    }
    return res;
}

const char *config_getCredentialsEncoded(const char *userWithPasswd)
{
    static char *result;
    unsigned userNameLen = strcspn(userWithPasswd, ":");
    MemBuf *mb = mb_new();

    mb_appendData(mb, userWithPasswd, userNameLen + 1);
    mb_appendStr(mb, FM_REALM);
    mb_appendStr(mb, userWithPasswd + userNameLen);
    result = realloc(result, userNameLen + 33);
    memcpy(result, userWithPasswd, userNameLen);
    md5_calculate(result + userNameLen, mb_data(mb), mb_dataLen(mb));
    mb_free(mb);

    return result;
}

bool config_isActionAvailable(enum PrivilegedAction pa)
{
    switch( pa ) {
    case PA_SERVE_PAGE:
        return true;
        break;
    case PA_LIST_FOLDER:
        return gAvailOps != DO_NONE;
    default:    /* PA_MODIFY */
        break;
    }
    return gAvailOps == DO_ALL;
}

bool config_isActionAllowed(enum PrivilegedAction pa, bool isLoggedIn)
{
    if( ! config_isActionAvailable(pa) )
        return false;
    if( isLoggedIn || gGuestOps == DO_ALL )
        return true;
    switch( pa ) {
    case PA_SERVE_PAGE:
        return gGuestOps != DO_NONE;
    case PA_LIST_FOLDER:
        return gGuestOps == DO_LISTING;
    default:    /* PA_MODIFY */
        break;
    }
    return false;
}

bool config_givesLoginMorePrivileges(void)
{
    switch( gGuestOps ) {
    case DO_ALL:
        return false;
    case DO_LISTING:
        return gAvailOps == DO_ALL;
    case DO_FILE:
        return gAvailOps != DO_NONE;
    default:    /* DO_NONE */
        break;
    }
    return true;
}

unsigned config_getMaxClients(void)
{
    return gMaxClients;
}

