#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include "filemanager.h"
#include "fmconfig.h"
#include "datachunk.h"
#include "folder.h"

/*#define DEBUG*/
#define DEBUG

#ifdef DEBUG
static void dolog(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}
#else
#define dolog(...)
#endif


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
    "        th.firstElementChild.textContent = \"-\";\n"
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

static RespBuf *printMovedAddSlash(const char *urlPath, int onlyHead)
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

static void print_error(RespBuf *resp, const char *errDetail)
{
    if( errDetail != NULL ) {
        resp_appendStrL(resp,
                "<h3 style=\"text-align: center; background-color: gold\">",
                escapeHtml(errDetail, 0), "</h3>\n", NULL);
    }
}

static RespBuf *printMesgPage(HttpStatus status, const char *mesg,
        const char *path, int onlyHead)
{
    char hostname[HOST_NAME_MAX];
    RespBuf *resp;

    resp = resp_new(status);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( ! onlyHead ) {
        gethostname(hostname, sizeof(hostname));
        resp_appendStrL(resp, "<html><head><title>", escapeHtml(path, 0),
            " on ", escapeHtml(hostname, 0), "</title></head><body>\n", NULL);
        print_error(resp, resp_getErrorMessage(resp));
        if( mesg != NULL ) {
            resp_appendStrL(resp, "<p>", escapeHtml(mesg, 0), "</p>", NULL);
        }
        resp_appendStr(resp, "</body></html>\n");
    }
    return resp;
}

static RespBuf *printErrorPage(int sysErrno, const char *path,
        int onlyHead)
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
    return printMesgPage(status, mesg, path, onlyHead);
}

static RespBuf *printFolderContents(const char *urlPath, const Folder *folder,
        int isModifiable, const char *opErrorMsg, int onlyHead)
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
    /* error bar */
    print_error(resp, opErrorMsg);
    /* host name as link to root */
    resp_appendStrL(resp, "<h3><a href=\"/\">", hostname_esc, "</a>&emsp;",
            NULL);
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
    resp_appendStr(resp, "</h3>\n<table><tbody>\n");
    /* link to parent - " .. " */
    if( urlPathLen > 1 ) {
        resp_appendStr(resp, "<tr>\n"
             "<td><span class=\"plusgray\">+</span></td>\n"
             "<td><a style=\"white-space: pre\" href=\"");
        resp_appendData(resp, urlPathEsc, strrchr(urlPathEsc, '/')-urlPathEsc);
        resp_appendStr(resp, "/\"> .. </a></td><td></td>\n</tr>\n");
    }
    /* entry list */
    for(cur_ent = folder_getEntries(folder); cur_ent->fileName; ++cur_ent) {
        /* red plus */
        resp_appendStrL(resp, "<tr><td onclick=\"showOptions(this)\"><span "
                "class=\"", cur_ent->isDir ? "plusdir" : "plusfile",
                "\">+</span></td>", NULL);
        /* entry name as link */
        fname_esc = escapeHtml(cur_ent->fileName, 0);
        resp_appendStrL(resp, "<td><a href=\"", urlPathEsc,
                urlPathLen == 1 ? "" : "/", fname_esc,
                cur_ent->isDir ? "/" : "", "\">", fname_esc, "</a></td>", NULL);
        if( cur_ent->isDir )
            resp_appendStr(resp, "<td></td>");
        else{
            char buf[20];
            sprintf(buf, "%u", (cur_ent->size+1023) / 1024);
            resp_appendStrL(resp, "<td style=\"text-align: right\">", buf,
                    " kB</td>", NULL);
        }
        resp_appendStr(resp, "</tr>");

        /* menu displayed after click red plus */
        resp_appendStr(resp, "<tr style=\"display: none\"><td></td>\n");
        if( isModifiable ) {
            resp_appendStrL(resp, "<td colspan=\"2\"><form method=\"POST\" "
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
                    "/</option>\n", NULL);
            for(optent = folder_getEntries(folder); optent->fileName; ++optent){
                if( optent != cur_ent && optent->isDir ) {
                    resp_appendStrL(resp, "<option>", urlPathEsc, "/",
                            escapeHtml(optent->fileName, 0), "/</option>\n",
                            NULL);
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
                    "</form></td>\n", NULL);
        }else{
            resp_appendStr(resp,
                    "<td colspan=\"2\">No action possible</td>\n");
        }
        resp_appendStr(resp, "</tr>\n");
    }
    /* footer */
    resp_appendStrL(resp, "</tbody></table>",
            isModifiable ? response_footer : "", "</body></html>\n", NULL);
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
    dolog("getContentTypeByFileExt: ext=%s, mime=%s\n", ext, res);
    return res;
}

static RespBuf *send_file(const char *urlPath, const char *sysPath,
        int onlyHead)
{
    FILE *fp;
    char buf[65536];
    int rd;
    RespBuf *resp;

    if( (fp = fopen(sysPath, "r")) != NULL ) {
        dolog("send_file: opened %s", sysPath);
        resp = resp_new(HTTP_200_OK);
        resp_appendHeader(resp, "Content-Type",
                getContentTypeByFileExt(sysPath));
        if( ! onlyHead ) {
            // TODO: escape filename
            sprintf(buf, "inline; filename=\"%s\"", strrchr(urlPath, '/')+1);
            resp_appendHeader(resp, "Content-Disposition", buf);
            while( (rd = fread(buf, 1, sizeof(buf), fp)) > 0 ) {
                resp_appendData(resp, buf, rd);
            }
        }
        fclose(fp);
    }else{
        resp = printErrorPage(errno, urlPath, onlyHead);
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
    dch_Clear(&contentDisp);
    while( res->data.len >= 2 && ! dch_StartsWithStr(&res->data, "\r\n") ) {
        if( dch_StartsWithStr(&res->data, "Content-Disposition:") ) {
            if( ! dch_ExtractTillStr(&res->data, &contentDisp, "\r\n") )
                return 0;
        }else{
            if( ! dch_ShiftAfterStr(&res->data, "\r\n") )
                return 0;
        }
    }
    if( ! dch_Shift(&res->data, 2) )
        return 0;
    if( contentDisp.data == NULL ) {
        dolog("no Content-Disposition in part");
        return 0;
    }
    /* Content-Disposition: form-data; name="file"; filename="Test.xml" */
    dch_Clear(&res->name);
    dch_Clear(&res->filename);
    while( dch_ShiftAfterChr(&contentDisp, ';') ) {
        dch_SkipLeading(&contentDisp, " ");
        if( ! dch_ExtractTillStr(&contentDisp, &name, "=") )
            return 0;
        if( ! dch_StartsWithStr(&contentDisp, "\"") )
            return 0;
        dch_Shift(&contentDisp, 1);
        if( ! dch_ExtractTillStr(&contentDisp, &value, "\"") )
            return 0;
        if( dch_EqualsStr(&name, "name") ) {
            res->name = value;
        }else if( dch_EqualsStr(&name, "filename") ) {
            res->filename = value;
        }
    }
    return res->name.data != NULL;
}

static MemBuf *fmtError(int sysErrno, const char *str1, const char *str2, ...)
{
    va_list args;
    MemBuf *mb;

    mb = mb_new();
    mb_appendStr(mb, str1);
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

    fname = dch_DupToStr(dchfname);
    dolog("upload_file: adding file=%s len=%u\n", fname, data->len);
    if( dch_EqualsStr(dchfname, "") ) {
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
        old_name = dch_DupToStr(dch_old_name);
        new_dir = dch_DupToStr(dch_new_dir);
        new_name = dch_DupToStr(dch_new_name);
        oldpath = format_path(sysPath, old_name);
        newUrlPath = format_path(new_dir, new_name);
        newSysPath = config_getSysPathForUrlPath(newUrlPath);
        dolog("rename_file: %s -> %s", oldpath, newSysPath);
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
    }else if( dch_IndexOfStr(dchNewDir, "/") >= 0 ) {
        res = fmtError(0, "directory name cannot contain slashes"
                "&emsp;&ndash;&emsp;\"/\"", NULL);
    }else{
        newDir = dch_DupToStr(dchNewDir);
        path = format_path(sysPath, newDir);
        dolog("create dir: %s", path);
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
        fname = dch_DupToStr(dch_fname);
        path = format_path(sysPath, fname);
        dolog("delete: %s", path);
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

static MemBuf *process_post(const char *sysPath, const char *ct,
        const MemBuf *requestBody)
{
    DataChunk bodyData, partData;
    struct content_part part, file_part, newdir_part, newname_part;
    enum { RT_UNKNOWN, RT_UPLOAD, RT_RENAME, RT_NEWDIR, RT_DELETE }
        request_type = RT_UNKNOWN;
    MemBuf *res = NULL;

    dch_Clear(&file_part.name);
    dch_Clear(&newdir_part.name);
    dch_Clear(&newname_part.name);
    if( mb_dataLen(requestBody) == 0 ) {
        res = fmtError(0, "bad content length: 0", NULL);
        goto err;
    }
    if( ct == NULL ) {
        res = fmtError(0, "unspecified content type", NULL);
        goto err;
    }
    if( memcmp(ct, "multipart/form-data; boundary=", 30) ) {
        res = fmtError(0, "bad content type: ", ct, NULL);
        goto err;
    }
    ct += 30;
    /*dolog("boundary: %s", ct);*/
    dch_Init(&bodyData, mb_data(requestBody), mb_dataLen(requestBody));
    /* skip first boundary */
    if( ! dch_ExtractTillStr2(&bodyData, &partData, "--", ct) ) {
        res = fmtError(0, "malformed form data", NULL);
        goto err;
    }
    /* check string after boundary; string after last boundary is "--\r\n" */
    while( dch_StartsWithStr(&bodyData, "\r\n") ) {
        dch_Shift(&bodyData, 2);
        if( ! dch_ExtractTillStr2(&bodyData, &partData, "\r\n--", ct) )
            break;
        if( parse_part(&partData, &part) ) {
            /*
            if( part.filename.data != NULL )
                dolog("part: {name=%.*s, filename=%.*s, datalen=%u}",
                        part.name.len, part.name.data,
                        part.filename.len, part.filename.data,
                        part.data.len);
            else
                dolog("part: {name=%.*s, datalen=%u}",
                        part.name.len, part.name.data, part.data.len);
            */
            if( dch_EqualsStr(&part.name, "do_upload") ) {
                request_type = RT_UPLOAD;
            }else if( dch_EqualsStr(&part.name, "do_rename") ) {
                request_type = RT_RENAME;
            }else if( dch_EqualsStr(&part.name, "do_newdir") ) {
                request_type = RT_NEWDIR;
            }else if( dch_EqualsStr(&part.name, "do_delete") ) {
                request_type = RT_DELETE;
            }else if( dch_EqualsStr(&part.name, "file") ) {
                file_part = part;
            }else if( dch_EqualsStr(&part.name, "new_dir") ) {
                newdir_part = part;
            }else if( dch_EqualsStr(&part.name, "new_name") ) {
                newname_part = part;
            }
        }else{
            res = fmtError(0, "malformed form data", NULL);
            goto err;
        }
        if(  dch_StartsWithStr(&bodyData, "--\r\n") )
            break;
    }
    switch( request_type ) {
    case RT_UPLOAD:
        res = upload_file(sysPath, &file_part.filename,
                &file_part.data);
        break;
    case RT_RENAME:
        res = rename_file(sysPath, &file_part.data,
                &newdir_part.data, &newname_part.data);
        break;
    case RT_NEWDIR:
        res = create_newdir(sysPath, &newdir_part.data);
        break;
    case RT_DELETE:
        res = delete_file(sysPath, &file_part.data);
        break;
    default:
        res = fmtError(0, "unrecognized request", NULL);
        break;
    }
err:
    return res;
}

RespBuf *filemgr_processRequest(const RequestBuf *req)
{
    unsigned queryFileLen, isHeadReq, isPostReq;
    int sysErrNo;
    const char *queryFile;
    RespBuf *resp;
    struct stat st;

    isHeadReq = !strcmp(req_getMethod(req), "HEAD");
    isPostReq = !strcmp(req_getMethod(req), "POST");
    queryFile = req_getPath(req);
    queryFileLen = strlen(queryFile);
    if( queryFileLen >= 3 && (strstr(queryFile, "/../") != NULL ||
            !strcmp(queryFile+queryFileLen-3, "/.."))) 
    {
        resp = printMesgPage(HTTP_403_FORBIDDEN, NULL, queryFile, isHeadReq);
    }else{
        char *sysPath = config_getSysPathForUrlPath(queryFile);
        Folder *folder = NULL;
        int isFolder;

        resp = NULL;
        sysErrNo = 0;
        if( sysPath != NULL ) {
            if( stat(sysPath, &st) == 0 )
                isFolder = S_ISDIR(st.st_mode);
            else
                sysErrNo = errno;
        }else{
            if( (folder = config_getSubSharesForPath(queryFile)) == NULL )
                sysErrNo = ENOENT;
            else
                isFolder = 1;
        }

        if( sysErrNo == 0 && isFolder && queryFile[queryFileLen-1] != '/' ) {
            resp = printMovedAddSlash(queryFile, isHeadReq);
        }else{
            char *opErrorMsg = NULL;
            if( sysErrNo == 0 ) {
                if( isPostReq ) {
                    if( sysPath != NULL ) {
                        MemBuf *opErrBuf = process_post(sysPath,
                                req_getHeaderVal(req, "Content-Type"),
                                req_getBody(req));
                        if( opErrBuf != NULL ) {
                            mb_appendData(opErrBuf, "", 1);
                            opErrorMsg = mb_unbox_free(opErrBuf);
                        }
                    }else{
                        opErrorMsg = strdup("request not allowed here");
                    }
                }
                if( isFolder && folder == NULL ) {
                    char *indexFile = config_getIndexFile(sysPath, &sysErrNo);
                    if( indexFile != NULL ) {
                        free(sysPath);
                        sysPath = indexFile;
                    }else if( sysErrNo == 0 ) {
                        if( config_isDirListingAllowed() )
                            folder = folder_loadDir(sysPath, &sysErrNo);
                        else
                            sysErrNo = ENOENT;
                    }
                }
            }
            if( sysErrNo == 0 ) {
                if( isFolder ) {
                    resp = printFolderContents(queryFile, folder,
                            sysPath == NULL ? 0 : access(sysPath, W_OK) == 0,
                            opErrorMsg, isHeadReq);
                }else if( opErrorMsg == NULL ) {
                    resp = send_file(queryFile, sysPath, isHeadReq);
                }else{
                    resp = printMesgPage(HTTP_200_OK, opErrorMsg,
                            queryFile, isHeadReq);
                }
            }else{
                resp = printErrorPage(sysErrNo, queryFile, isHeadReq);
            }
            free(opErrorMsg);
        }
        folder_free(folder);
        free(sysPath);
    }
    return resp;
}

