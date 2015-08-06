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


/* Note: the may return argument itself (if does not need escape).
 * The argument cannot be freed while result is used.
 */
static const char *escapeHtml(const char *s, int isInJavascriptStr)
{
    enum { ESCMAX = 6 };
    static char *bufs[ESCMAX];
    static int next = 0;
    int len, alloc;
    char *res, *repl;

    len = strcspn(s, "\"'&<>");
    if( s[len] == '\0' )
        return s;
    res = bufs[next];
    alloc = strlen(s) + 20;
    res = realloc(res, alloc);
    memcpy(res, s, len);
    s += len;
    while( *s ) {
        if( alloc - len < 10 ) {
            alloc += 20;
            res = realloc(res, alloc);
        }
        repl = NULL;
        switch( *s ) {
        case '"':
            repl = isInJavascriptStr ? "\\&quot;" : "&quot;";
            break;
        case '\'':
            repl = isInJavascriptStr ? "\\&apos;" : "&apos;";
            break;
        case '&':
            repl = "&amp;";
            break;
        case '<':
            repl = "&lt;";
            break;
        case '>':
            repl = "&gt;";
            break;
        case '\\':
            repl = isInJavascriptStr ? "\\\\" : "\\";
            break;
        default:
            res[len++] = *s;
        }
        if( repl != NULL ) {
            strcpy(res+len, repl);
            len += strlen(repl);
        }
        ++s;
    }
    res[len] = '\0';
    bufs[next] = res;
    next = next == ESCMAX-1 ? 0 : next+1;
    return res;
}

static RespBuf *printMovedAddSlash(const char *urlPath, bool onlyHead)
{
    char hostname[HOST_NAME_MAX], *newPath;
    const char *escaped;
    int len;
    RespBuf *resp;

    resp = resp_new(HTTP_301_MOVED_PERMANENTLY);
    len = strlen(urlPath);
    newPath = malloc(len+2);
    memcpy(newPath, urlPath, len);
    strcpy(newPath+len, "/");
    resp_appendHeader(resp, "Location", newPath);
    free(newPath);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( ! onlyHead ) {
        escaped = escapeHtml(urlPath, 0);
        gethostname(hostname, sizeof(hostname));
        resp_appendStrL(resp, "<html><head><title>", escaped,
            " on ", escapeHtml(hostname, 0),
            "</title></head><body>\n<h3>Moved to <a href=\"", escaped,
            "/\">", escaped, "/</a></h3>\n</body></html>\n", NULL);
    }
    return resp;
}

static RespBuf *printMesgPage(HttpStatus status, const char *mesg,
        const char *path, bool onlyHead, bool showLoginButton)
{
    char hostname[HOST_NAME_MAX];
    RespBuf *resp;

    resp = resp_new(status);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( ! onlyHead ) {
        gethostname(hostname, sizeof(hostname));
        resp_appendStrL(resp, "<html><head><title>", escapeHtml(path, 0),
            " on ", escapeHtml(hostname, 0), "</title></head><body>\n", NULL);
        if( showLoginButton )
            resp_appendStr(resp, response_login_button);
        resp_appendStrL(resp,
            "&nbsp;<div style=\"text-align: center; margin: 150px 0px\">\n"
            "<span style=\"font-size: x-large; border: 1px solid #FFF0B0; "
            "background-color: #FFFCF0; padding: 50px 100px\">\n",
            escapeHtml(resp_getErrorMessage(resp), 0),
            "</span></div>\n", NULL);
        if( mesg != NULL )
            resp_appendStrL(resp, "<p>", escapeHtml(mesg, 0), "</p>", NULL);
        resp_appendStr(resp, "</body></html>\n");
    }
    return resp;
}

static RespBuf *printErrorPage(int sysErrno, const char *path,
        bool onlyHead, bool showLoginButton)
{
    HttpStatus status;
    const char *mesg = NULL;

    switch(sysErrno) {
    case 0:
        status = HTTP_200_OK;
        break;
    case ENOENT:
    case ENOTDIR:
        status = HTTP_404_NOT_FOUND;
        break;
    case EPERM:
    case EACCES:
        status = HTTP_403_FORBIDDEN;
        break;
    default:
        status = HTTP_500;
        mesg = strerror(sysErrno);
        break;
    }
    return printMesgPage(status, mesg, path, onlyHead, showLoginButton);
}

static RespBuf *printUnauthorized(const char *urlPath, bool onlyHead)
{
    char *authHeader;
    RespBuf *resp;

    resp = printMesgPage(HTTP_401_UNAUTHORIZED, NULL, urlPath, onlyHead, false);
    authHeader = auth_getAuthResponseHeader();
    resp_appendHeader(resp, "WWW-Authenticate", authHeader);
    free(authHeader);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    return resp;
}

static RespBuf *printFolderContents(const char *urlPath, const Folder *folder,
        bool isModifiable, bool showLoginButton, const char *opErrorMsg,
        bool onlyHead)
{
    char hostname[HOST_NAME_MAX];
    char *urlPathNoSl;
    const char *s, *sn, *hostname_esc, *fname_esc, *urlPathEsc;
    const FolderEntry *cur_ent, *optent;
    int urlPathLen;
    RespBuf *resp;

    urlPathLen = strlen(urlPath);
    while( urlPathLen > 1 && urlPath[urlPathLen-1] == '/' )
        --urlPathLen;
    urlPathNoSl = malloc(urlPathLen+1);
    memcpy(urlPathNoSl, urlPath, urlPathLen);
    urlPathNoSl[urlPathLen] = '\0';
    urlPathEsc = escapeHtml(urlPathNoSl, 0);
    gethostname(hostname, sizeof(hostname));
    hostname_esc = escapeHtml(hostname, 0);
    resp = resp_new(HTTP_200_OK);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( onlyHead )
        return resp;
    /* head, title */
    resp_appendStrL(resp, "<html><head><title>", urlPathEsc, " on ",
            hostname_esc, "</title>", response_header, "</head>\n<body>\n",
            NULL);
    /* host name as link to root */
    resp_appendStrL(resp, "<span style=\"font-size: large; font-weight: bold\">"
            "<a href=\"/\">", hostname_esc, "</a>&emsp;", NULL);
    /* current path as link list */
    if( urlPathLen > 1 ) {
        for(s = urlPathEsc+1; (sn = strchr(s, '/')) != NULL; s = sn+1) {
            resp_appendStr(resp, "/<a href=\"");
            resp_appendData(resp, urlPathEsc, sn-urlPathEsc);
            resp_appendStr(resp, "/\">");
            resp_appendData(resp, s, sn-s);
            resp_appendStr(resp, "</a>");
        }
        resp_appendStrL(resp, "/<a href=\"", urlPathEsc, "/\">", s, "</a>",
                NULL);
    }
    resp_appendStr(resp, "</span>\n");
    if( showLoginButton )
        resp_appendStr(resp, response_login_button);
    /* error bar */
    if( opErrorMsg != NULL ) {
        resp_appendStrL(resp,
                "<div style=\"font-size: large; text-align: center; "
                "background-color: gold; padding: 2px; margin-top: 4px\">",
                escapeHtml(opErrorMsg, 0), "</div>\n", NULL);
    }
    resp_appendStr(resp, "<table><tbody>\n");
    /* link to parent - " .. " */
    if( urlPathLen > 1 ) {
        resp_appendStrL(resp, "<tr>\n<td><span class=\"plusgray\">",
                isModifiable ? "+" : "&sdot;", "</span></td>\n"
                 "<td><a style=\"white-space: pre\" href=\"", NULL);
        resp_appendData(resp, urlPathEsc, strrchr(urlPathEsc, '/')-urlPathEsc);
        resp_appendStr(resp, "/\"> .. </a></td><td></td>\n</tr>\n");
    }
    /* entry list */
    for(cur_ent = folder_getEntries(folder); cur_ent->fileName; ++cur_ent) {
        /* colored square */
        if( isModifiable ) {
            resp_appendStrL(resp, "<tr><td onclick=\"showOptions(this)\"><span "
                    "class=\"", cur_ent->isDir ? "plusdir" : "plusfile",
                    "\">+</span></td>", NULL);
        }else{
            resp_appendStrL(resp, "<tr><td><span class=\"",
                    cur_ent->isDir ? "plusdir" : "plusfile",
                    "\">&sdot;</span></td>", NULL);
        }
        /* entry name as link */
        fname_esc = escapeHtml(cur_ent->fileName, 0);
        resp_appendStrL(resp, "<td><a href=\"", urlPathEsc,
                urlPathLen == 1 ? "" : "/", fname_esc,
                cur_ent->isDir ? "/" : "", "\">", fname_esc, "</a></td>", NULL);
        /* optional entry size */
        if( cur_ent->isDir )
            resp_appendStr(resp, "<td></td>");
        else{
            char buf[20];
            sprintf(buf, "%llu", (cur_ent->size+1023) / 1024);
            resp_appendStrL(resp, "<td style=\"text-align: right\">", buf,
                    " kB</td>", NULL);
        }
        resp_appendStr(resp, "</tr>");

        /* menu displayed after click red plus */
        if( isModifiable ) {
            resp_appendStrL(resp,
                    "<tr style=\"display: none\"><td></td>\n"
                    "<td colspan=\"2\"><form method=\"POST\" "
                    "enctype=\"multipart/form-data\">"
                    "<input type=\"hidden\" name=\"file\" value=\"",
                    fname_esc, "\"/>new name:<select name=\"new_dir\">\n",
                    NULL);
            for(s = urlPathEsc; s != NULL && s[1]; s = strchr(s+1, '/') ) {
                resp_appendStr(resp, "<option>");
                resp_appendData(resp, urlPathEsc, s - urlPathEsc);
                resp_appendStr(resp, "/</option>\n");
            }
            resp_appendStrL(resp, "<option selected>",  urlPathEsc,
                    urlPathLen == 1 ? "" : "/", "</option>\n", NULL);
            for(optent = folder_getEntries(folder); optent->fileName; ++optent){
                if( optent != cur_ent && optent->isDir ) {
                    resp_appendStrL(resp, "<option>", urlPathEsc,
                            urlPathLen == 1 ? "" : "/",
                            escapeHtml(optent->fileName, 0),
                            "/</option>\n", NULL);
                }
            }
            resp_appendStrL(resp,
                    "</select><input name=\"new_name\" value=\"", fname_esc,
                    "\"/><input type=\"submit\" name=\"do_rename\" "
                    "value=\"Rename\"/><br/>\n"
                    "<input type=\"submit\" name=\"do_delete\" "
                    "value=\"Delete\" onclick=\"return confirm("
                    "'delete &quot;", escapeHtml(cur_ent->fileName, 1),
                    "&quot; ?')\"/>\n"
                    "</form></td>\n</tr>\n", NULL);
        }
    }
    /* footer */
    resp_appendStrL(resp, "</tbody></table>",
            isModifiable ? response_footer : "", "</body></html>\n", NULL);
    free(urlPathNoSl);
    return resp;
}

static const char *getContentTypeByFileExt(const char *fname)
{
    static struct mime_type { const char *ext, *mime; } mime_types[] = {
        { "aac",     "audio/x-hx-aac-adts" },
        { "avi",     "video/x-msvideo" },
        { "bmp",     "image/bmp" },
        { "bz2",     "application/x-bzip2" },
        { "c",       "text/x-c" },
        { "css",     "text/css" },
        { "deb",     "application/x-debian-package" },
        { "doc",     "application/msword" },
        { "flv",     "video/x-flv" },
        { "gif",     "image/gif" },
        { "gz",      "application/gzip" },
        { "htm",     "text/html; charset=utf-8" },
        { "html",    "text/html; charset=utf-8" },
        { "java",    "text/plain" },
        { "jar",     "application/jar" },
        { "jpe",     "image/jpeg" },
        { "jpeg",    "image/jpeg" },
        { "jpg",     "image/jpeg" },
        { "m3u",     "audio/x-mpegurl" },
        { "mid",     "audio/midi" },
        { "midi",    "audio/midi" },
        { "mov",     "video/quicktime" },
        { "mp2",     "audio/mpeg" },
        { "mp3",     "audio/mpeg" },
        { "mp4",     "video/mp4" },
        { "mpe",     "video/mpeg" },
        { "mpeg",    "video/mpeg" },
        { "mpg",     "video/mpeg" },
        { "ogg",     "application/x-ogg" },
        { "pdf",     "application/pdf" },
        { "png",     "image/png" },
        { "ppt",     "application/vnd.ms-powerpoint" },
        { "ps",      "application/postscript" },
        { "qt",      "video/quicktime" },
        { "ra",      "audio/x-realaudio" },
        { "ram",     "audio/x-pn-realaudio" },
        { "rtf",     "text/rtf" },
        { "sh",      "text/plain; charset=utf-8" },
        { "svg",     "image/svg+xml" },
        { "svgz",    "image/svg+xml" },
        { "tar",     "application/x-tar" },
        { "tif",     "image/tiff" },
        { "tiff",    "image/tiff" },
        { "tsv",     "text/tab-separated-values; charset=utf-8" },
        { "txt",     "text/plain; charset=utf-8" },
        { "wav",     "audio/x-wav" },
        { "wma",     "audio/x-ms-wma" },
        { "wmv",     "video/x-ms-wmv" },
        { "wmx",     "video/x-ms-wmx" },
        { "xls",     "application/vnd.ms-excel" },
        { "xml",     "text/xml; charset=utf-8" },
        { "xpm",     "image/x-xpmi" },
        { "xsl",     "text/xml; charset=utf-8" },
        { "zip",     "application/zip" }
    };
    const char *ext, *res = NULL;
    int i;

    if( (ext = strrchr(fname, '/')) == NULL )
        ext = fname;
    if( (ext = strrchr(ext, '.')) == NULL || ext == fname || ext[-1] == '/' )
        ext = ".txt";
    ++ext;
    for(i = 0; i < sizeof(mime_types) / sizeof(mime_types[0]) &&
            res == NULL; ++i)
    {
        if( !strcasecmp(ext, mime_types[i].ext) )
            res = mime_types[i].mime;
    }
    if( res == NULL )
        res = "application/octet-stream";
    log_debug("ext=%s, mime=%s", ext, res);
    return res;
}

static RespBuf *sendFile(const char *urlPath, const char *sysPath,
        bool onlyHead)
{
    int fd;
    RespBuf *resp;

    if( (fd = open(sysPath, O_RDONLY)) >= 0 ) {
        log_debug("opened %s", sysPath);
        resp = resp_new(HTTP_200_OK);
        resp_appendHeader(resp, "Content-Type",
                getContentTypeByFileExt(sysPath));
        if( onlyHead ) {
            close(fd);
        }else{
            // TODO: escape filename
            MemBuf *header = mb_new();
            mb_appendStrL(header, "inline; filename=\"", 
                    strrchr(urlPath, '/')+1, "\"", NULL);
            resp_appendHeader(resp, "Content-Disposition", mb_data(header));
            mb_free(header);
            resp_enqFile(resp, fd);
        }
    }else{
        resp = printErrorPage(errno, urlPath, onlyHead, false);
    }
    return resp;
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

static bool processPost(const RequestBuf *req, const char *sysPath,
        char **errMsgBuf)
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
    const char *ct = req_getHeaderVal(req, "Content-Type");
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
        requireAuth = req_getLoginState(req) != LS_LOGGED_IN;
    }else if( requestType >= RT_MODIFY_BEG &&
            config_isActionAvailable(PA_MODIFY) )
    {
        requireAuth = !req_isActionAllowed(req, PA_MODIFY);
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
    return requireAuth;
}

static RespBuf *processFolderReq(const RequestBuf *req, const char *sysPath,
        const Folder *folder)
{
    char *opErrorMsg = NULL;
    const char *queryFile = req_getPath(req);
    RespBuf *resp;
    bool isHeadReq = !strcmp(req_getMethod(req), "HEAD");
    int sysErrNo = 0;
    Folder *toFree = NULL;

    if( !strcmp(req_getMethod(req), "POST") &&
                processPost(req, sysPath, &opErrorMsg) )
    {
        resp = printUnauthorized(queryFile, isHeadReq);
    }else{
        if( req_isActionAllowed(req, PA_LIST_FOLDER) ) {
            bool isModifiable = sysPath == NULL ? 0 :
                req_isActionAllowed(req, PA_MODIFY) &&
                    access(sysPath, W_OK) == 0;
            if( folder == NULL )
                folder = toFree = folder_loadDir(sysPath, &sysErrNo);
            if( sysErrNo == 0 ) {
                resp = printFolderContents(queryFile, folder, isModifiable,
                        req_isWorthPuttingLogOnButton(req),
                        opErrorMsg, isHeadReq);
            }else{
                resp = printErrorPage(ENOENT, queryFile, isHeadReq, false);
            }
        }else{
            resp = printErrorPage(ENOENT, queryFile, isHeadReq,
                    req_isWorthPuttingLogOnButton(req));
        }
    }
    free(opErrorMsg);
    folder_free(toFree);
    return resp;
}

RespBuf *filemgr_processRequest(const RequestBuf *req)
{
    unsigned queryFileLen, isHeadReq;
    const char *queryFile;
    RespBuf *resp;

    isHeadReq = !strcmp(req_getMethod(req), "HEAD");
    queryFile = req_getPath(req);
    queryFileLen = strlen(queryFile);
    if( req_getLoginState(req) == LS_LOGIN_FAIL ||
            ! req_isActionAllowed(req, PA_SERVE_PAGE) )
    {
        if( req_getLoginState(req) == LS_LOGIN_FAIL ) {
            log_debug("authorization fail: sleep 2");
            sleep(2); /* make dictionary attack harder */
        }
        resp = printUnauthorized(req_getPath(req), isHeadReq);
    }else if( queryFileLen >= 3 && (strstr(queryFile, "/../") != NULL ||
            !strcmp(queryFile+queryFileLen-3, "/.."))) 
    {
        resp = printMesgPage(HTTP_403_FORBIDDEN, NULL, queryFile,
                isHeadReq, false);
    }else{
        int sysErrNo = 0;
        struct stat st;
        char *sysPath = config_getSysPathForUrlPath(queryFile), *indexFile;
        Folder *folder = NULL;
        bool isFolder = false;

        if( sysPath != NULL ) {
            if( stat(sysPath, &st) == 0 )
                isFolder = S_ISDIR(st.st_mode);
            else
                sysErrNo = errno;
        }else{
            if( (folder = config_getSubSharesForPath(queryFile)) == NULL )
                sysErrNo = ENOENT;
            else
                isFolder = true;
        }

        if( sysErrNo == 0 && isFolder && queryFile[strlen(queryFile)-1] != '/')
        {
            resp = printMovedAddSlash(queryFile, isHeadReq);
        }else{
            if( sysErrNo == 0 && isFolder && folder == NULL &&
                (indexFile = config_getIndexFile(sysPath, &sysErrNo)) != NULL )
            {
                free(sysPath);
                sysPath = indexFile;
                isFolder = false;
            }
            if( sysErrNo == 0 ) {
                if( isFolder ) {
                    resp = processFolderReq(req, sysPath, folder);
                }else{
                    resp = sendFile(queryFile, sysPath, isHeadReq);
                }
            }else{
                resp = printErrorPage(sysErrNo, queryFile, isHeadReq, false);
            }
        }
        folder_free(folder);
        free(sysPath);
    }
    log_debug("response: %s", resp_getErrorMessage(resp));
    return resp;
}

