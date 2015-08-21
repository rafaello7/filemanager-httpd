#include <stdbool.h>
#include "filemanager.h"
#include "fmconfig.h"
#include "datachunk.h"
#include "folder.h"
#include "auth.h"
#include "fmlog.h"
#include "multipartdata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>


struct FileManager {
    char *sysPath;
    MultipartData *body;
    char *opErrorMsg;
};

static char *format_path(const char *rootdir, const char *subdir)
{
    char *res;
    int rootdir_len, subdir_len;

    rootdir_len = strlen(rootdir);
    subdir_len = strlen(subdir);
    res = malloc( rootdir_len + subdir_len + 2 );
    memcpy(res, rootdir, rootdir_len);
    if( res[rootdir_len-1] != '/' )
        res[rootdir_len++] = '/';
    if( subdir[0] == '/' ) {
        ++subdir;
        --subdir_len;
    }
    memcpy(res + rootdir_len, subdir, subdir_len + 1);
    return res;
}

static const char response_header[] =
    "<script>\n"
    "function showOptions(th) {\n"
    "    th.parentNode.nextElementSibling.style.display = \"table-row\";\n"
    "    th.firstElementChild.innerHTML = \"&minus;\";\n"
    "    th.onclick = function() { hideOptions(th); };\n"
    "}\n"
    "function hideOptions(th) {\n"
    "    th.parentNode.nextElementSibling.style.display = \"none\";\n"
    "    th.firstElementChild.textContent = \"+\";\n"
    "    th.onclick = function() { showOptions(th); };\n"
    "}\n"
    "function confirmRepl(th) {\n"
    "    var fname = th.form.elements.namedItem('file').value;\n"
    "    var repl = th.form.elements.namedItem('new_cont').value;\n"
    "    if( repl == '' ) {\n"
    "        alert('please choose a file');\n"
    "        return false;\n"
    "    }\n"
    "    repl = repl.replace(/.*[\\/\\\\]/, '');\n"
    "    repl = repl == fname ? '' : ' using \"' + repl + '\"';\n"
    "    return confirm('replace \"' + fname+ '\"' + repl + ' ?');\n"
    "}\n"
    "function confirmDel(th) {\n"
    "    var fname = th.form.elements.namedItem('file').value;\n"
    "    var recu = th.form.elements.namedItem('del_recursive');\n"
    "    recu = recu != null && recu.checked ? ' recursively' : '';\n"
    "    return confirm('delete' + recu + ' \"' + fname + '\" ?');\n"
    "}\n"
    "function showHideHidden(th) {\n"
    "    var rows = document.getElementsByTagName('tr');\n"
    "    for(var i = 0; i < rows.length; ++i) {\n"
    "        var row = rows.item(i);\n"
    "        if( row.getAttribute('class') == 'rhidden' ) {\n"
    "            row.style.display = th.checked ? 'table-row' : 'none';\n"
    "        }\n"
    "    }\n"
    "}\n"
    "function checkCreateDir(th) {\n"
    "    if( th.form.elements.namedItem('new_dir').value == '' ) {\n"
    "        alert('please specify the directory name');\n"
    "        return false;\n"
    "    }\n"
    "    return true;\n"
    "}\n"
    "function checkAddFile(th) {\n"
    "    if( th.form.elements.namedItem('file').value == '' ) {\n"
    "        alert('please choose a file');\n"
    "        return false;\n"
    "    }\n"
    "    return true;\n"
    "}\n"
    "</script>\n"
    "<style>\n"
    "    body { background-color: #F2FAFC; }\n"
    "    span.plusdir {\n"
    "        font-family: monospace;\n"
    "        font-weight: bold;\n"
    "        background-color: #D31D41;\n"
    "        color: white;\n"
    "        padding: 0px 3px;\n"
    "        cursor: default;\n"
    "    }\n"
    "    span.plusgray {\n"
    "        font-family: monospace;\n"
    "        font-weight: bold;\n"
    "        background-color: #d8d8d8;\n"
    "        color: white;\n"
    "        padding: 0px 3px;\n"
    "        cursor: default;\n"
    "    }\n"
    "    span.plusfile {\n"
    "        font-family: monospace;\n"
    "        font-weight: bold;\n"
    "        background-color: #bbdb1e;\n"
    "        color: white;\n"
    "        padding: 0px 3px;\n"
    "        cursor: default;\n"
    "    }\n"
    "    tbody.folder > tr > td {\n"
    "        border-color: #ded4f2;\n"
    "        border-width: 1px;\n"
    "        border-bottom-style: solid;\n"
    "    }\n"
    "    table.fattr {\n"
    "        border-collapse: collapse;\n"
    "    }\n"
    "    table.fattr td {\n"
    "        background: #e0f0f9;\n"
    "        padding: 3px 6px;\n"
    "    }\n"
    "    table.fattr input[type='submit'] {\n"
    "        width: 100%;\n"
    "    }\n"
    "    table.fattr input[name='new_name'] {\n"
    "        width: 20em;\n"
    "    }\n"
    "    table.diracns td {\n"
    "        padding: 2px 4px;\n"
    "    }\n"
    "    table.diracns input {\n"
    "        width: 100%;\n"
    "    }\n"
    "</style>\n";

static const char response_login_button[] =
    "<form style='display: inline' method=\"POST\" "
    "enctype=\"multipart/form-data\">\n"
    "<input type=\"submit\" name=\"do_login\" value=\"Login\">\n"
    "</form>\n";

static const char response_footer[] =
    "<p><hr></p>\n"
    "<form method='POST' enctype='multipart/form-data'>\n"
    "<table class='diracns'><tbody>\n"
    "<tr><td>new directory:</td>\n"
    "<td><input name='new_dir'/></td>\n"
    "<td><input type='submit' name='do_newdir' value='Create' "
    "onclick='return checkCreateDir(this)'/></td>\n"
    "</tr><tr><td>add file:</td>\n"
    "<td><input type='file' name='file'/></td>\n"
    "<td><input type='submit' name='do_add' value='Add' "
    "onclick='return checkAddFile(this)'/></td>\n"
    "</tr></tbody></table></form>\n";


static const char *gFilePermDisp[] = {
    "---", "r--", "-w-", "rw-", "--x", "r-x", "-wx", "rwx"
};
enum {
    PERM_DISP_CNT = sizeof(gFilePermDisp)/sizeof(gFilePermDisp[0]),
    PERM_GROUP_USER = 0,
    PERM_GROUP_GROUP,
    PERM_GROUP_OTHERS,
    PERM_GROUP_COUNT
};
static const struct {
    const char *name;
    unsigned mask;
    unsigned value[PERM_DISP_CNT];
} gFilePerm[PERM_GROUP_COUNT] = {
    { "user", S_IRWXU,
        { 0, S_IRUSR, S_IWUSR, S_IRUSR|S_IWUSR, S_IXUSR,
            S_IRUSR|S_IXUSR, S_IWUSR|S_IXUSR, S_IRUSR|S_IWUSR|S_IXUSR }
    },
    { "group", S_IRWXG,
        { 0, S_IRGRP, S_IWGRP, S_IRGRP|S_IWGRP, S_IXGRP,
            S_IRGRP|S_IXGRP, S_IWGRP|S_IXGRP, S_IRGRP|S_IWGRP|S_IXGRP }
    },
    { "others", S_IRWXO,
        { 0, S_IROTH, S_IWOTH, S_IROTH|S_IWOTH, S_IXOTH,
            S_IROTH|S_IXOTH, S_IWOTH|S_IXOTH, S_IROTH|S_IWOTH|S_IXOTH }
    }
};

const char *filemgr_getLoginForm(void)
{
    return response_login_button;
}

static MemBuf *fmtError(int sysErrno, const char *str1, const char *str2, ...)
{
    va_list args;
    MemBuf *mb;

    mb = mb_newWithStr(str1);
    if( str2 != NULL ) {
        va_start(args, str2);
        while( str2 != NULL ) {
            mb_appendStr(mb, str2);
            str2 = va_arg(args, const char*);
        }
        va_end(args);
    }
    if( sysErrno != 0 ) {
        mb_appendStr(mb, ": ");
        mb_appendStr(mb, strerror(sysErrno));
    }
    return mb;
}


static MemBuf *add_new_file(ContentPart *partFile)
{
    MemBuf *res = NULL;
    const char *fname = cpart_getFileName(partFile);
    int sysErrNo;

    if( fname != NULL ) {
        if( ! cpart_finishUpload(partFile, NULL, false, &sysErrNo) )
            res = fmtError(sysErrNo, "unable to store ", fname, NULL);
    }else
        res = fmtError(0, "unable to add: no file chosen", NULL);
    return res;
}

static MemBuf *rename_file(const char *sysPath, const char *oldName,
        const char *newDir, const char *newName)
{
    char *oldpath, *newUrlPath, *newSysPath;
    MemBuf *res = NULL;

    if( oldName == NULL || newDir == NULL || newName == NULL ) {
        res = fmtError(0, "not all parameters provided for rename", NULL);
    }else{
        oldpath = format_path(sysPath, oldName);
        newUrlPath = format_path(newDir, newName);
        newSysPath = config_getSysPathForUrlPath(newUrlPath);
        log_debug("rename_file: %s -> %s", oldpath, newSysPath);
        if( rename(oldpath, newSysPath) != 0 ) {
            res = fmtError(errno, oldName, " rename failed", NULL);
        }
        free(oldpath);
        free(newSysPath);
        free(newUrlPath);
    }
    return res;
}

static MemBuf *create_newdir(const char *sysPath, const char *newDir)
{
    char *path;
    MemBuf *res = NULL;

    if( newDir == NULL || newDir[0] == '\0' ) {
        res = fmtError(0, "unable to create directory with empty name", NULL);
    }else if( strchr(newDir, '/') != NULL ) {
        res = fmtError(0, "directory name cannot contain slashes"
                "&emsp;&ndash;&emsp;\"/\"", NULL);
    }else{
        path = format_path(sysPath, newDir);
        log_debug("create dir: %s", path);
        if( mkdir(path, S_IRWXU|S_IRWXG|S_IRWXO) != 0 ) {
            res = fmtError(errno, "unable to create directory \"", newDir,
                    "\"", NULL);
        }
        free(path);
    }
    return res;
}

static int del_recursive(const char *path, int *sysErrNo)
{
    int res = 0, dirNameLen;
    DIR *d;
    struct dirent *dp;

    log_debug("del_recursive: removing %s", path);
    if( unlink(path) != 0 ) {
        if( errno == EISDIR ) {
            if( (d = opendir(path)) != NULL ) {
                MemBuf *filePathName = mb_newWithStr(path);
                mb_ensureEndsWithSlash(filePathName);
                dirNameLen = mb_dataLen(filePathName);
                while( res == 0 && (dp = readdir(d)) != NULL ) {
                    if( strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) {
                        mb_setStrEnd(filePathName, dirNameLen, dp->d_name);
                        res = del_recursive(mb_data(filePathName), sysErrNo);
                    }
                }
                closedir(d);
                mb_free(filePathName);
                if( res == 0 ) {
                    res = rmdir(path);
                    *sysErrNo = errno;
                }
            }else{
                *sysErrNo = errno;
                res = -1;
            }
        }else{
            *sysErrNo = errno;
            res = errno == ENOENT ? 0 : -1;
        }
    }
    return res;
}

static MemBuf *delete_file(const char *sysPath, const char *fname,
        bool recursively)
{
    char *path;
    MemBuf *res = NULL;
    int opRes, sysErrNo;

    if( fname == NULL || fname[0] == '\0' ) {
        res = fmtError(0, "unable to remove file with empty name", NULL);
    }else{
        path = format_path(sysPath, fname);
        if( recursively ) {
            log_debug("delete recursively: %s", path);
            opRes = del_recursive(path, &sysErrNo);
        }else{
            log_debug("delete: %s", path);
            if( (opRes = unlink(path)) != 0 && errno == EISDIR )
                opRes = rmdir(path);
            sysErrNo = errno;
        }
        if( opRes != 0 )
            res = fmtError(sysErrNo, "delete ", fname, " failed", NULL);
        free(path);
    }
    return res;
}

static MemBuf *replace_file(const char *sysPath, const char *fname,
        ContentPart *partFile)
{
    MemBuf *res = NULL;
    const char *tmpPath = cpart_getFilePathName(partFile);
    int sysErrNo;

    if( fname != NULL && tmpPath != NULL ) {
        char *filePath = format_path(sysPath, fname);
        if( ! cpart_finishUpload(partFile, filePath, true, &sysErrNo) )
            res = fmtError(sysErrNo, "unable to store ", fname, NULL);
        free(filePath);
    }else
        res = fmtError(0, "unable to replace: no file chosen", NULL);
    return res;
}

static MemBuf *chmod_file(const char *sysPath, const char *fname,
        const char *puser, const char *pgroup, const char *pothers)
{
    char *path;
    const char *perm[PERM_GROUP_COUNT] = { puser, pgroup, pothers };
    MemBuf *res = NULL;
    unsigned i, j, mode = 0;
    bool isValidParam = fname && puser && pgroup && pothers;

    if( isValidParam ) {
        for( i = 0; i < PERM_GROUP_COUNT && isValidParam; ++i) {
            for(j = 0; j < PERM_DISP_CNT &&
                    strcmp(perm[i], gFilePermDisp[j]); ++j)
                ;
            if( j < PERM_DISP_CNT )
                mode |= gFilePerm[i].value[j];
            else
                isValidParam = false;
        }
        if( isValidParam ) {
            path = format_path(sysPath, fname);
            log_debug("change mode to 0%o (%s%s%s) of %s",
                    mode, puser, pgroup, pothers, path);
            if( chmod(path, mode) != 0 )
                res = fmtError(errno, "change permissions of ", fname,
                        " failed", NULL);
            free(path);
        }
    }
    if( ! isValidParam )
        res = fmtError(0, "invalid parameters", NULL);
    return res;
}

enum PostingResult filemgr_processPost(FileManager *filemgr,
        const RequestHeader *rhdr)
{
    ContentPart *file_part, *newdir_part, *newname_part, *newcont_part;
    ContentPart *puser_part, *pgroup_part, *pothers_part;
    MemBuf *opErr = NULL;
    bool requireAuth = false;

    if( filemgr->body == NULL )
        return PR_PROCESSED;

    if( mpdata_containsPartWithName(filemgr->body, "do_login") ) {
        requireAuth = reqhdr_getLoginState(rhdr) != LS_LOGGED_IN;
    }else if( config_isActionAvailable(PA_MODIFY) ) {
        requireAuth = !reqhdr_isActionAllowed(rhdr, PA_MODIFY);
        if( ! requireAuth ) {
            file_part = mpdata_getPartByName(filemgr->body, "file");
            newdir_part = mpdata_getPartByName(filemgr->body, "new_dir");
            newname_part = mpdata_getPartByName(filemgr->body, "new_name");
            puser_part = mpdata_getPartByName(filemgr->body, "puser");
            pgroup_part = mpdata_getPartByName(filemgr->body, "pgroup");
            pothers_part = mpdata_getPartByName(filemgr->body, "pothers");
            newcont_part = mpdata_getPartByName(filemgr->body, "new_cont");
            if( mpdata_containsPartWithName(filemgr->body, "do_add") ) {
                opErr = add_new_file(file_part);
            }else if(mpdata_containsPartWithName(filemgr->body, "do_rename")) {
                opErr = rename_file(filemgr->sysPath,
                        cpart_getDataStr(file_part),
                        cpart_getDataStr(newdir_part), 
                        cpart_getDataStr(newname_part));
            }else if(mpdata_containsPartWithName(filemgr->body, "do_newdir")) {
                opErr = create_newdir(filemgr->sysPath,
                        cpart_getDataStr(newdir_part));
            }else if(mpdata_containsPartWithName(filemgr->body, "do_replace")) {
                opErr = replace_file(filemgr->sysPath,
                        cpart_getDataStr(file_part), newcont_part);
            }else if(mpdata_containsPartWithName(filemgr->body, "do_delete")) {
                opErr = delete_file(filemgr->sysPath,
                        cpart_getDataStr(file_part),
                        mpdata_getPartByName(filemgr->body,
                            "del_recursive") != NULL);
            }else if(mpdata_containsPartWithName(filemgr->body, "do_perm")) {
                opErr = chmod_file(filemgr->sysPath,
                        cpart_getDataStr(file_part),
                        cpart_getDataStr(puser_part),
                        cpart_getDataStr(pgroup_part),
                        cpart_getDataStr(pothers_part));
            }
        }
    }else
        opErr = fmtError(0, "unrecognized request", NULL);
    mpdata_free(filemgr->body);
    filemgr->body = NULL;
    filemgr->opErrorMsg = mb_unbox_free(opErr);
    return requireAuth ? PR_REQUIRE_AUTH : PR_PROCESSED;
}

static bool hasHiddenFiles(const Folder *folder)
{
    const FolderEntry *cur_ent;

    for(cur_ent = folder_getEntries(folder); cur_ent->fileName; ++cur_ent) {
        if( cur_ent->fileName[0] == '.' )
            return true;
    }
    return false;
}

static RespBuf *printFolderContents(const char *urlPath, const Folder *folder,
        bool isModifiable, bool showLoginButton, const char *opErrorMsg,
        bool onlyHead)
{
    char hostname[HOST_NAME_MAX];
    const FolderEntry *cur_ent, *optent;
    DataChunk dchUrlPath, dchDirName;
    RespBuf *resp;
    unsigned pathElemBeg, pathElemEnd, i, j;

    resp = resp_new(resp_cmnStatus(HTTP_200_OK), onlyHead);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( onlyHead )
        return resp;
    dch_initWithStr(&dchUrlPath, urlPath);
    dch_trimTrailing(&dchUrlPath, '/');
    gethostname(hostname, sizeof(hostname));
    /* head, title */
    resp_appendStr(resp, "<html><head><title>");
    if( dchUrlPath.len ) {
        resp_appendChunkEscapeHtml(resp, &dchUrlPath);
        resp_appendStr(resp, " on ");
    }
    resp_appendStrEscapeHtml(resp, hostname);
    resp_appendStrL(resp, " - File Manager</title>", response_header,
            "</head>\n<body>\n", NULL);
    /* host name as link to root */
    resp_appendStr(resp, "<table style='width: 100%'><tbody><tr>");
    resp_appendStr(resp, "<td style=\"font-size: large; font-weight: bold\">"
            "<a href=\"/\">");
    resp_appendStrEscapeHtml(resp, hostname);
    resp_appendStr(resp, "</a>&emsp;");
    /* current path as link list */
    pathElemBeg = dch_endOfSpan(&dchUrlPath, 0, '/');
    while( dchUrlPath.len > pathElemBeg ) {
        pathElemEnd = dch_endOfCSpan(&dchUrlPath, pathElemBeg, '/');
        resp_appendStr(resp, "/<a href=\"");
        resp_appendDataEscapeHtml(resp, dchUrlPath.data, pathElemEnd);
        resp_appendStr(resp, "/\">");
        resp_appendDataEscapeHtml(resp, dchUrlPath.data + pathElemBeg,
                pathElemEnd - pathElemBeg);
        resp_appendStr(resp, "</a>");
        pathElemBeg = dch_endOfSpan(&dchUrlPath, pathElemEnd, '/');
    }
    resp_appendStr(resp, "</td><td style='text-align: right'>");
    if( hasHiddenFiles(folder) )
        resp_appendStr(resp, "<label><input type='checkbox' name='showall' "
                "onclick='showHideHidden(this)'></input>show hidden files"
                "</label>");
    if( showLoginButton )
        resp_appendStrL(resp, "&emsp;", response_login_button, NULL);
    resp_appendStr(resp, "</td></tr></tbody></table>\n");
    /* error bar */
    if( opErrorMsg != NULL ) {
        resp_appendStr(resp,
                "<div style=\"font-size: large; text-align: center; "
                "background-color: gold; padding: 2px; margin-top: 4px\">");
        resp_appendStrEscapeHtml(resp, opErrorMsg);
        resp_appendStr(resp, "</div>\n");
    }
    resp_appendStr(resp, "<table><tbody class='folder'>\n");
    /* link to parent - " .. " */
    if( dchUrlPath.len ) {
        resp_appendStrL(resp, "<tr>\n<td><span class=\"plusgray\">",
                isModifiable ? "+" : "&sdot;", "</span></td>\n"
                 "<td><a style=\"white-space: pre\" href=\"", NULL);
        dch_dirNameOf(&dchUrlPath, &dchDirName);
        if( ! dch_equalsStr(&dchDirName, "/") )
            resp_appendChunkEscapeHtml(resp, &dchDirName);
        resp_appendStr(resp, "/\"> .. </a></td>\n<td></td>\n</tr>\n");
    }
    /* entry list */
    for(cur_ent = folder_getEntries(folder); cur_ent->fileName; ++cur_ent) {
        if( cur_ent->fileName[0] == '.' )
            resp_appendStr(resp,"<tr class='rhidden' style='display: none'>\n");
        else
            resp_appendStr(resp, "<tr>\n");
        /* colored square */
        if( isModifiable ) {
            resp_appendStrL(resp, "<td onclick=\"showOptions(this)\">"
                    "<span class=\"", cur_ent->isDir ? "plusdir" : "plusfile",
                    "\">+</span></td>\n", NULL);
        }else{
            resp_appendStrL(resp, "<td><span class=\"",
                    cur_ent->isDir ? "plusdir" : "plusfile",
                    "\">&sdot;</span></td>\n", NULL);
        }
        /* entry name as link */
        resp_appendStr(resp, "<td><a href=\"");
        resp_appendChunkEscapeHtml(resp, &dchUrlPath);
        resp_appendStr(resp, "/");
        resp_appendStrEscapeHtml(resp, cur_ent->fileName);
        if( cur_ent->isDir )
            resp_appendStr(resp, "/");
        resp_appendStr(resp, "\">");
        resp_appendStrEscapeHtml(resp, cur_ent->fileName);
        resp_appendStr(resp, "</a></td>\n");
        /* optional entry size */
        if( cur_ent->isDir )
            resp_appendStr(resp, "<td></td>\n");
        else{
            static const char spc[] = "&thinsp;";
            char buf[80];
            int len, dest = sizeof(buf)-1, cpy;

            sprintf(buf, "%llu", (cur_ent->size+1023) / 1024);
            /* insert thin spaces every three digits */
            len = strlen(buf);
            buf[dest] = '\0';
            while( len > 0 ) {
                cpy = sizeof(spc) - 1;
                dest -= cpy;
                memcpy(buf+dest, spc, cpy);
                cpy = len > 3 ? 3 : len;
                dest -= cpy;
                len -= cpy;
                memcpy(buf+dest, buf+len, cpy);
            }
            resp_appendStrL(resp, "<td style=\"text-align: right; "
                    "padding-left: 2em; white-space: nowrap\">",
                    buf + dest, "kB</td>\n", NULL);
        }
        resp_appendStr(resp, "</tr>\n");

        /* menu displayed after click red plus */
        if( isModifiable ) {
            resp_appendStr(resp,
                    "<tr style=\"display: none\">\n"
                    "<td></td>\n"
                    "<td colspan=\"2\">\n"
                    "<form method=\"POST\" enctype=\"multipart/form-data\">\n"
                    "<input type=\"hidden\" name=\"file\" value=\"");
            resp_appendStrEscapeHtml(resp, cur_ent->fileName);
            resp_appendStr(resp, "\"/>\n<table class='fattr'><tbody>");
            /* first row - "new name:" */
            resp_appendStr(resp, "<tr><td>new name:</td>\n"
                    "<td colspan='3'><select name='new_dir'>\n");
            pathElemEnd = 0;
            while( pathElemEnd < dchUrlPath.len ) {
                pathElemBeg = dch_endOfSpan(&dchUrlPath, pathElemEnd, '/');
                resp_appendStr(resp, "<option>");
                resp_appendDataEscapeHtml(resp, dchUrlPath.data, pathElemBeg);
                resp_appendStr(resp, "</option>\n");
                pathElemEnd = dch_endOfCSpan(&dchUrlPath, pathElemBeg, '/');
            }
            resp_appendStr(resp, "<option selected>");
            resp_appendChunkEscapeHtml(resp, &dchUrlPath);
            resp_appendStr(resp, "/");
            resp_appendStr(resp, "</option>\n");
            for(optent = folder_getEntries(folder); optent->fileName; ++optent){
                if( optent != cur_ent && optent->isDir ) {
                    resp_appendStr(resp, "<option>");
                    resp_appendChunkEscapeHtml(resp, &dchUrlPath);
                    resp_appendStr(resp, "/");
                    resp_appendStrEscapeHtml(resp, optent->fileName);
                    resp_appendStr(resp, "/</option>\n");
                }
            }
            resp_appendStr(resp, "</select> <input name='new_name' value=\"");
            resp_appendStrEscapeHtml(resp, cur_ent->fileName);
            resp_appendStr(resp, "\"/></td>"
                    "<td><input type=\"submit\" "
                    "name=\"do_rename\" value=\"Rename\"/></td></tr>\n");
            /* second row - "permissions:" */
            resp_appendStr(resp, "<tr><td>permissions:</td>");
            for(i = 0; i < PERM_GROUP_COUNT; ++i) {
                resp_appendStrL(resp, "<td>", gFilePerm[i].name,
                        ": <select name='p", gFilePerm[i].name, "'>\n", NULL);
                for(j = 0; j < PERM_DISP_CNT; ++j) {
                    resp_appendStrL(resp, "<option", 
                            (cur_ent->mode & gFilePerm[i].mask) ==
                            gFilePerm[i].value[j] ? " selected" : "", ">",
                            gFilePermDisp[j], "</option>\n", NULL);
                }
                resp_appendStr(resp, "</select></td>\n");
            }
            resp_appendStr(resp, "<td><input type=\"submit\" "
                    "name='do_perm' value='Change'/></td></tr>\n");
            /* 3rd row - "replace with:" */
            if( ! cur_ent->isDir ) {
                resp_appendStr(resp, "<tr><td>replace with:</td>\n"
                        "<td colspan='3'><input type='file' name='new_cont'>"
                        "</td><td><input type='submit' name='do_replace' "
                        "value='Upload' onclick='return confirmRepl(this)'/>"
                        "</td></tr>\n");
            }
            /* 4th row - "delete:" */
            resp_appendStr(resp, "<tr><td>delete:</td>\n<td colspan='3'>");
            if( cur_ent->isDir ) {
                resp_appendStr(resp, "<label>"
                    "<input type='checkbox' name='del_recursive'/>"
                    "recursively</label>");
            }
            resp_appendStr(resp, "</td>\n"
                    "<td><input type=\"submit\" name=\"do_delete\" "
                    "value=\"Delete\" onclick=\"return confirmDel(this)\"/>"
                    "</td>\n</tr></tbody>\n</table>\n</form>\n</td>\n</tr>\n");
        }
    }
    /* footer */
    resp_appendStrL(resp, "</tbody></table>",
            isModifiable ? response_footer : "", "</body></html>\n", NULL);
    return resp;
}

RespBuf *filemgr_printFolderContents(const FileManager *filemgr,
        const RequestHeader *rhdr, int *sysErrNo)
{
    Folder *folder;
    const char *queryFile = reqhdr_getPath(rhdr);
    bool isHeadReq = !strcmp(reqhdr_getMethod(rhdr), "HEAD");
    RespBuf *resp = NULL;
    bool isModifiable = filemgr->sysPath == NULL ? 0 :
        reqhdr_isActionAllowed(rhdr, PA_MODIFY) &&
            access(filemgr->sysPath, W_OK) == 0;

    if( filemgr->sysPath != NULL )
        folder = folder_loadDir(filemgr->sysPath, sysErrNo);
    else if( (folder = config_getSubSharesForPath(queryFile)) == NULL )
        *sysErrNo = ENOENT;
    if( *sysErrNo == 0 ) {
        resp = printFolderContents(queryFile, folder,
                isModifiable, reqhdr_isWorthPuttingLogOnButton(rhdr),
                filemgr->opErrorMsg, isHeadReq);
    }
    folder_free(folder);
    return resp;
}

FileManager *filemgr_new(const char *sysPath, const RequestHeader *rhdr)
{
    FileManager *filemgr = malloc(sizeof(FileManager));
    DataChunk dchContentType, dchName, dchValue;
    MemBuf *opErr = NULL;
    const char *contentType = reqhdr_getHeaderVal(rhdr, "Content-Type");
    char *boundaryDelimiter = NULL;

    filemgr->sysPath = sysPath ? strdup(sysPath) : NULL;
    if( contentType != NULL ) {
        dch_initWithStr(&dchContentType, contentType);
        dch_extractTillChrStripWS(&dchContentType, &dchName, ';');
        if( ! dch_equalsStr(&dchName, "multipart/form-data") ) {
            opErr = fmtError(0, "bad content type: ", contentType, NULL);
        }else{
            while(dch_extractParam(&dchContentType, &dchName, &dchValue, ';')) {
                if( dch_equalsStrIgnoreCase(&dchName, "boundary") )
                    boundaryDelimiter = dch_dupToStr(&dchValue);
            }
        }
    }
    if( boundaryDelimiter != NULL ) {
        filemgr->body = mpdata_new(boundaryDelimiter,
                reqhdr_isActionAllowed(rhdr, PA_MODIFY) ? sysPath : NULL);
        free(boundaryDelimiter);
    }else
        filemgr->body = NULL;
    filemgr->opErrorMsg = mb_unbox_free(opErr);
    return filemgr;
}

void filemgr_consumeBodyBytes(FileManager *filemgr, const char *data,
        unsigned len)
{
    if( filemgr->body != NULL )
        mpdata_appendData(filemgr->body, data, len);
}

void filemgr_free(FileManager *filemgr)
{
    if( filemgr != NULL ) {
        free(filemgr->sysPath);
        mpdata_free(filemgr->body);
        free(filemgr->opErrorMsg);
        free(filemgr);
    }
}

