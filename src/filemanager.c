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
    "    function showOptions(th) {\n"
    "        th.parentNode.nextElementSibling.style.display = \"table-row\";\n"
    "        th.firstElementChild.innerHTML = \"&minus;\";\n"
    "        th.onclick = function() { hideOptions(th); };\n"
    "    }\n"
    "    function hideOptions(th) {\n"
    "        th.parentNode.nextElementSibling.style.display = \"none\";\n"
    "        th.firstElementChild.textContent = \"+\";\n"
    "        th.onclick = function() { showOptions(th); };\n"
    "    }\n"
    "    function confirmDel(th) {\n"
    "        while( th != null && th.name != 'file' )\n"
    "            th = th.previousElementSibling;\n"
    "        return th != null && confirm('delete \"' + th.value + '\" ?');\n"
    "    }\n"
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
    "    td {\n"
    "        border-color: #ded4f2;\n"
    "        border-width: 1px;\n"
    "        border-bottom-style: solid;\n"
    "    }\n"
    "</style>\n";

static const char response_login_button[] =
    "<form style=\"float: right\" method=\"POST\" "
    "enctype=\"multipart/form-data\">\n"
    "<input type=\"submit\" name=\"do_login\" value=\"Login\">\n"
    "</form>\n";

static const char response_footer[] =
    "<p><hr></p>\n"
    "<form method=\"POST\" enctype=\"multipart/form-data\">\n"
    "<table><tbody>\n"
    "<tr><td style=\"text-align: right\">new directory:</td>\n"
    "<td><input style=\"width: 100%\" name=\"new_dir\"/></td>\n"
    "<td><input style=\"width: 100%\" type=\"submit\" name=\"do_newdir\" "
    "value=\"Create\"/></td></tr>\n"
    "<tr><td style=\"text-align: right\">add file:</td>\n"
    "<td><input type=\"file\" name=\"file\"/></td>\n"
    "<td><input style=\"width: 100%\" type=\"submit\" name=\"do_upload\" "
    "value=\"Add\"/></td></tr>\n"
    "</tbody></table></form>\n";


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


static MemBuf *upload_file(ContentPart *partFile)
{
    MemBuf *res = NULL;
    const char *fname = cpart_getFileName(partFile);
    int sysErrNo;

    if( fname != NULL ) {
        const char *pathName = cpart_getFilePathName(partFile);
        if( cpart_finishUpload(partFile, &sysErrNo) ) {
            log_debug("upload: added file %s", pathName);
        }else{
            if( pathName != NULL )
                log_debug("upload: failed to create %s, errno=%d (%s)",
                        pathName, sysErrNo, strerror(sysErrNo));
            else
                log_debug("upload: no target dir for %s", fname);
            res = fmtError(sysErrNo, "unable to save ", fname, NULL);
        }
    }else
        res = fmtError(0, "unable to add file with empty name", NULL);
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

static MemBuf *delete_file(const char *sysPath, const char *fname)
{
    char *path;
    struct stat st;
    MemBuf *res = NULL;
    int opRes;

    if( fname == NULL || fname[0] == '\0' ) {
        res = fmtError(0, "unable to remove file with empty name", NULL);
    }else{
        path = format_path(sysPath, fname);
        log_debug("delete: %s", path);
        if( stat(path, &st) == 0 ) {
            if( S_ISDIR(st.st_mode) ) {
                opRes = rmdir(path);
            }else{
                opRes = unlink(path);
            }
            if( opRes != 0 ) {
                res = fmtError(errno, "delete ", fname, " failed", NULL);
            }
        }else{
            res = fmtError(errno, fname, NULL);
        }
        free(path);
    }
    return res;
}

enum PostingResult filemgr_processPost(FileManager *filemgr,
        const RequestHeader *rhdr)
{
    ContentPart *file_part, *newdir_part, *newname_part;
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
            if( mpdata_containsPartWithName(filemgr->body, "do_upload") ) {
                opErr = upload_file(file_part);
            }else if(mpdata_containsPartWithName(filemgr->body, "do_rename")) {
                opErr = rename_file(filemgr->sysPath,
                        cpart_getDataStr(file_part),
                        cpart_getDataStr(newdir_part), 
                        cpart_getDataStr(newname_part));
            }else if(mpdata_containsPartWithName(filemgr->body, "do_newdir")) {
                opErr = create_newdir(filemgr->sysPath,
                        cpart_getDataStr(newdir_part));
            }else if(mpdata_containsPartWithName(filemgr->body, "do_delete")) {
                opErr = delete_file(filemgr->sysPath,
                        cpart_getDataStr(file_part));
            }
        }
    }else
        opErr = fmtError(0, "unrecognized request", NULL);
    filemgr->opErrorMsg = mb_unbox_free(opErr);
    return requireAuth ? PR_REQUIRE_AUTH : PR_PROCESSED;
}

static RespBuf *printFolderContents(const char *urlPath, const Folder *folder,
        bool isModifiable, bool showLoginButton, const char *opErrorMsg,
        bool onlyHead)
{
    char hostname[HOST_NAME_MAX];
    const FolderEntry *cur_ent, *optent;
    DataChunk dchUrlPath, dchDirName;
    RespBuf *resp;
    unsigned pathElemBeg, pathElemEnd;

    resp = resp_new(HTTP_200_OK, onlyHead);
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
    resp_appendStr(resp, "<span style=\"font-size: large; font-weight: bold\">"
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
    resp_appendStr(resp, "</span>\n");
    if( showLoginButton )
        resp_appendStr(resp, response_login_button);
    /* error bar */
    if( opErrorMsg != NULL ) {
        resp_appendStr(resp,
                "<div style=\"font-size: large; text-align: center; "
                "background-color: gold; padding: 2px; margin-top: 4px\">");
        resp_appendStrEscapeHtml(resp, opErrorMsg);
        resp_appendStr(resp, "</div>\n");
    }
    resp_appendStr(resp, "<table><tbody>\n");
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
        /* colored square */
        if( isModifiable ) {
            resp_appendStrL(resp, "<tr>\n<td onclick=\"showOptions(this)\">"
                    "<span class=\"", cur_ent->isDir ? "plusdir" : "plusfile",
                    "\">+</span></td>\n", NULL);
        }else{
            resp_appendStrL(resp, "<tr>\n<td><span class=\"",
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
                    "<tr style=\"display: none\"><td></td>\n"
                    "<td colspan=\"2\">\n<form method=\"POST\" "
                    "enctype=\"multipart/form-data\">"
                    "<input type=\"hidden\" name=\"file\" value=\"");
            resp_appendStrEscapeHtml(resp, cur_ent->fileName);
            resp_appendStr(resp, "\"/>new name:<select name=\"new_dir\">\n");
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
            resp_appendStr(resp, "</select><input name=\"new_name\" value=\"");
            resp_appendStrEscapeHtml(resp, cur_ent->fileName);
            resp_appendStr(resp, "\"/><input type=\"submit\" "
                    "name=\"do_rename\" value=\"Rename\"/><br/>\n"
                    "<input type=\"submit\" name=\"do_delete\" "
                    "value=\"Delete\" onclick=\"return confirmDel(this)\"/>\n"
                    "</form>\n</td>\n</tr>\n");
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
    RespBuf *resp;
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

