#include <stdbool.h>
#include "filemanager.h"
#include "fmconfig.h"
#include "datachunk.h"
#include "folder.h"
#include "auth.h"
#include "fmlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>


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

struct content_part {
    DataChunk name;
    DataChunk filename;
    DataChunk data;
};

static int parse_part(const DataChunk *part, struct content_part *res)
{
    DataChunk contentDisp, name, value;

    res->data = *part;
    dch_clear(&contentDisp);
    while( res->data.len >= 2 && ! dch_startsWithStr(&res->data, "\r\n") ) {
        if( dch_startsWithStr(&res->data, "Content-Disposition:") ) {
            if( ! dch_extractTillStr(&res->data, &contentDisp, "\r\n") )
                return 0;
        }else{
            if( ! dch_shiftAfterStr(&res->data, "\r\n") )
                return 0;
        }
    }
    if( ! dch_shift(&res->data, 2) )
        return 0;
    if( contentDisp.data == NULL ) {
        log_debug("no Content-Disposition in part");
        return 0;
    }
    /* Content-Disposition: form-data; name="file"; filename="Test.xml" */
    dch_clear(&res->name);
    dch_clear(&res->filename);
    while( dch_shiftAfterChr(&contentDisp, ';') ) {
        dch_skipLeading(&contentDisp, " ");
        if( ! dch_extractTillStr(&contentDisp, &name, "=") )
            return 0;
        if( ! dch_startsWithStr(&contentDisp, "\"") )
            return 0;
        dch_shift(&contentDisp, 1);
        if( ! dch_extractTillStr(&contentDisp, &value, "\"") )
            return 0;
        if( dch_equalsStr(&name, "name") ) {
            res->name = value;
        }else if( dch_equalsStr(&name, "filename") ) {
            res->filename = value;
        }
    }
    return res->name.data != NULL;
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


static MemBuf *upload_file(const char *sysPath, const DataChunk *dchfname,
        const DataChunk *data)
{
    FILE *fp;
    char *fname, *fname_real;
    MemBuf *res = NULL;

    fname = dch_dupToStr(dchfname);
    log_debug("upload_file: adding file=%s len=%u\n", fname, data->len);
    if( dch_equalsStr(dchfname, "") ) {
        res = fmtError(0, "unable to add file with empty name", NULL);
    }else{
        fname_real = format_path(sysPath, fname);
        if( access(fname_real, F_OK) == 0 ) {
            res = fmtError(0, "file ", fname, " already exists", NULL);
        }else{
            if( (fp = fopen(fname_real, "w")) == NULL ) {
                res = fmtError(errno, "unable to save ", fname, NULL);
            }else{
                if( data->len && fwrite(data->data, data->len, 1, fp) != 1 ) {
                    res = fmtError(errno, "error during save ", fname, NULL);
                    unlink(fname_real);
                }
                fclose(fp);
            }
        }
        free(fname_real);
    }
    free(fname);
    return res;
}

static MemBuf *rename_file(const char *sysPath, const DataChunk *dch_old_name,
        const DataChunk *dch_new_dir, const DataChunk *dch_new_name)
{
    char *old_name, *new_dir, *new_name, *oldpath, *newUrlPath, *newSysPath;
    MemBuf *res = NULL;

    if( dch_old_name->data == NULL || dch_new_dir->data == NULL ||
            dch_new_name->data == NULL )
    {
        res = fmtError(0, "not all parameters provided for rename", NULL);
    }else{
        old_name = dch_dupToStr(dch_old_name);
        new_dir = dch_dupToStr(dch_new_dir);
        new_name = dch_dupToStr(dch_new_name);
        oldpath = format_path(sysPath, old_name);
        newUrlPath = format_path(new_dir, new_name);
        newSysPath = config_getSysPathForUrlPath(newUrlPath);
        log_debug("rename_file: %s -> %s", oldpath, newSysPath);
        if( rename(oldpath, newSysPath) != 0 ) {
            res = fmtError(errno, old_name, " rename failed", NULL);
        }
        free(oldpath);
        free(newSysPath);
        free(newUrlPath);
        free(new_name);
        free(new_dir);
        free(old_name);
    }
    return res;
}

static MemBuf *create_newdir(const char *sysPath, const DataChunk *dchNewDir)
{
    char *path, *newDir;
    MemBuf *res = NULL;

    if( dchNewDir->len == 0 || dchNewDir->data[0] == '\0' ) {
        res = fmtError(0, "unable to create directory with empty name", NULL);
    }else if( dch_indexOfStr(dchNewDir, "/") >= 0 ) {
        res = fmtError(0, "directory name cannot contain slashes"
                "&emsp;&ndash;&emsp;\"/\"", NULL);
    }else{
        newDir = dch_dupToStr(dchNewDir);
        path = format_path(sysPath, newDir);
        log_debug("create dir: %s", path);
        if( mkdir(path, S_IRWXU|S_IRWXG|S_IRWXO) != 0 ) {
            res = fmtError(errno, "unable to create directory \"", newDir,
                    "\"", NULL);
        }
        free(path);
        free(newDir);
    }
    return res;
}

static MemBuf *delete_file(const char *sysPath, const DataChunk *dch_fname)
{
    char *path, *fname;
    struct stat st;
    MemBuf *res = NULL;
    int opRes;

    if( dch_fname->len == 0 || dch_fname->data[0] == '\0' ) {
        res = fmtError(0, "unable to remove file with empty name", NULL);
    }else{
        fname = dch_dupToStr(dch_fname);
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
        free(fname);
    }
    return res;
}

enum PostingResult filemgr_processPost(const RequestBuf *req,
        const char *sysPath, char **errMsgBuf)
{
    DataChunk bodyData, partData;
    struct content_part part, file_part, newdir_part, newname_part;
    enum {
        RT_UNKNOWN,
        RT_LOGIN,
        RT_MODIFY_BEG,  /* begin of requests requiring PA_MODIFY */
        RT_UPLOAD = RT_MODIFY_BEG,
        RT_RENAME,
        RT_NEWDIR,
        RT_DELETE
    } requestType = RT_UNKNOWN;
    MemBuf *opErr = NULL;
    const RequestHeader *rhdr = req_getHeader(req);
    const char *ct = reqhdr_getHeaderVal(rhdr, "Content-Type");
    const MemBuf *requestBody = req_getBody(req);
    bool requireAuth = false;

    dch_clear(&file_part.name);
    dch_clear(&newdir_part.name);
    dch_clear(&newname_part.name);
    if( mb_dataLen(requestBody) == 0 ) {
        opErr = fmtError(0, "bad content length: 0", NULL);
        goto err;
    }
    if( ct == NULL ) {
        opErr = fmtError(0, "unspecified content type", NULL);
        goto err;
    }
    if( memcmp(ct, "multipart/form-data; boundary=", 30) ) {
        opErr = fmtError(0, "bad content type: ", ct, NULL);
        goto err;
    }
    ct += 30;
    /*log_debug("boundary: %s", ct);*/
    dch_init(&bodyData, mb_data(requestBody), mb_dataLen(requestBody));
    /* skip first boundary */
    if( ! dch_extractTillStr2(&bodyData, &partData, "--", ct) ) {
        opErr = fmtError(0, "malformed form data", NULL);
        goto err;
    }
    /* check string after boundary; string after last boundary is "--\r\n" */
    while( dch_startsWithStr(&bodyData, "\r\n") ) {
        dch_shift(&bodyData, 2);
        if( ! dch_extractTillStr2(&bodyData, &partData, "\r\n--", ct) )
            break;
        if( parse_part(&partData, &part) ) {
            /*
            if( part.filename.data != NULL )
                log_debug("part: {name=%.*s, filename=%.*s, datalen=%u}",
                        part.name.len, part.name.data,
                        part.filename.len, part.filename.data,
                        part.data.len);
            else
                log_debug("part: {name=%.*s, datalen=%u}",
                        part.name.len, part.name.data, part.data.len);
            */
            if( dch_equalsStr(&part.name, "do_upload") ) {
                requestType = RT_UPLOAD;
            }else if( dch_equalsStr(&part.name, "do_rename") ) {
                requestType = RT_RENAME;
            }else if( dch_equalsStr(&part.name, "do_newdir") ) {
                requestType = RT_NEWDIR;
            }else if( dch_equalsStr(&part.name, "do_delete") ) {
                requestType = RT_DELETE;
            }else if( dch_equalsStr(&part.name, "do_login") ) {
                requestType = RT_LOGIN;
            }else if( dch_equalsStr(&part.name, "file") ) {
                file_part = part;
            }else if( dch_equalsStr(&part.name, "new_dir") ) {
                newdir_part = part;
            }else if( dch_equalsStr(&part.name, "new_name") ) {
                newname_part = part;
            }
        }else{
            opErr = fmtError(0, "malformed form data", NULL);
            goto err;
        }
        if(  dch_startsWithStr(&bodyData, "--\r\n") )
            break;
    }
    if( requestType == RT_LOGIN ) {
        requireAuth = reqhdr_getLoginState(rhdr) != LS_LOGGED_IN;
    }else if( requestType >= RT_MODIFY_BEG &&
            config_isActionAvailable(PA_MODIFY) )
    {
        requireAuth = !reqhdr_isActionAllowed(rhdr, PA_MODIFY);
        if( ! requireAuth ) {
            switch( requestType ) {
            case RT_UPLOAD:
                opErr = upload_file(sysPath, &file_part.filename,
                        &file_part.data);
                break;
            case RT_RENAME:
                opErr = rename_file(sysPath, &file_part.data,
                        &newdir_part.data, &newname_part.data);
                break;
            case RT_NEWDIR:
                opErr = create_newdir(sysPath, &newdir_part.data);
                break;
            case RT_DELETE:
                opErr = delete_file(sysPath, &file_part.data);
                break;
            default:
                break;
            }
        }
    }else
        opErr = fmtError(0, "unrecognized request", NULL);
err:
    *errMsgBuf = mb_unbox_free(opErr);
    return requireAuth ? PR_REQUIRE_AUTH : PR_PROCESSED;
}

RespBuf *filemgr_printFolderContents(const char *urlPath, const Folder *folder,
        bool isModifiable, bool showLoginButton, const char *opErrorMsg,
        bool onlyHead)
{
    char hostname[HOST_NAME_MAX];
    const FolderEntry *cur_ent, *optent;
    DataChunk dchUrlPath, dchDirName;
    RespBuf *resp;
    unsigned pathElemBeg, pathElemEnd;

    resp = resp_new(HTTP_200_OK);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( onlyHead )
        return resp;
    dch_initWithStr(&dchUrlPath, urlPath);
    dch_trimTrailing(&dchUrlPath, "/");
    gethostname(hostname, sizeof(hostname));
    /* head, title */
    resp_appendStr(resp, "<html><head><title>");
    resp_appendChunkEscapeHtml(resp, &dchUrlPath);
    resp_appendStr(resp, " on ");
    resp_appendStrEscapeHtml(resp, hostname);
    resp_appendStrL(resp, "</title>", response_header,
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

