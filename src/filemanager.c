#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "filemanager.h"
#include "configfile.h"
#include "datachunk.h"

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

static char *format_path3(const char *rootdir, const char *subdir1,
        const char *subdir2)
{
    char *path1, *path2;

    path1 = format_path(rootdir, subdir1);
    path2 = format_path(path1, subdir2);
    free(path1);
    return path2;
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
    "    body { background-color: #F7F5FC; }\n"
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

struct disp_entry {
    char *fname;
    int is_dir;
    unsigned size;
};

static int disp_ent_compare(const void *pvEnt1, const void *pvEnt2)
{
    const struct disp_entry *ent1 = pvEnt1;
    const struct disp_entry *ent2 = pvEnt2;

    if( ent1 ->is_dir != ent2->is_dir )
        return ent2->is_dir - ent1->is_dir;
    return strcoll(ent1->fname, ent2->fname);
}

static struct disp_entry *load_dir(const char *rootdir, const char *dname,
        int *is_modifiable, int *errNum)
{
    DIR *d;
    struct dirent *dp;
    struct stat st;
    char *dname_real, *fname_real;
    struct disp_entry *res = NULL;
    int count = 0, alloc = 0;

    dname_real = format_path(rootdir, dname);
    if( (d = opendir(dname_real)) != NULL ) {
        while( (dp = readdir(d)) != NULL ) {
            if( ! strcmp(dp->d_name, ".") || ! strcmp(dp->d_name, "..") )
                continue;
            fname_real = format_path(dname_real, dp->d_name);
            if( stat(fname_real, &st) == 0 ) {
                if( count == alloc ) {
                    alloc = alloc ? 2 * alloc : 8;
                    res = realloc(res, alloc * sizeof(struct disp_entry));
                }
                res[count].fname = strdup(dp->d_name);
                res[count].is_dir = S_ISDIR(st.st_mode);
                res[count].size = st.st_size;
                ++count;
            }
            free(fname_real);
        }
        closedir(d);
        qsort(res, count, sizeof(struct disp_entry), disp_ent_compare);
        res = realloc(res, (count+1) * sizeof(struct disp_entry));
        res[count].fname = NULL;
        *is_modifiable = access(dname_real, W_OK) == 0;
    }else{
        *errNum = errno;
        *is_modifiable = 0;
    }
    free(dname_real);
    return res;
}

static void free_loaded_dir(struct disp_entry *entries)
{
    struct disp_entry *cur_ent;

    for(cur_ent = entries; cur_ent->fname; ++cur_ent) {
        free(cur_ent->fname);
    }
    free(entries);
}

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

static void print_error(RespBuf *resp, const char *errDetail)
{
    if( errDetail != NULL ) {
        resp_appendStrL(resp,
                "<h3 style=\"text-align: center; background-color: gold\">",
                escapeHtml(errDetail, 0), "</h3>\n", NULL);
    }
}

static RespBuf *printErrorPage(int sysErrno, const char *share_name,
        int onlyHead)
{
    char hostname[HOST_NAME_MAX];
    RespBuf *resp;

    resp = resp_new(sysErrno);
    gethostname(hostname, sizeof(hostname));
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( ! onlyHead ) {
        resp_appendStrL(resp, "<html><head><title>", escapeHtml(share_name, 0),
            " on ", escapeHtml(hostname, 0), "</title></head><body>\n", NULL);
        print_error(resp, resp_getErrMessage(resp));
        resp_appendStr(resp, "</body></html>\n");
    }
    return resp;
}

static RespBuf *print_dir_contents(struct disp_entry *entries,
        int isModifiable, const char *share_name, const char *query_dir,
        const char *opErrorMsg, int onlyHead)
{
    char hostname[HOST_NAME_MAX];
    const char *s, *sn, *trailsl, *hostname_esc, *fname_esc;
    struct disp_entry *cur_ent, *optent;
    RespBuf *resp;

    trailsl = query_dir[strlen(query_dir)-1] == '/' ? "" : "/";
    gethostname(hostname, sizeof(hostname));
    share_name = escapeHtml(share_name, 0);
    query_dir = escapeHtml(query_dir, 0);
    hostname_esc = escapeHtml(hostname, 0);
    resp = resp_new(0);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( onlyHead )
        return resp;
    resp_appendStrL(resp, "<html><head><title>",
            share_name[0] ? share_name : query_dir, " on ", hostname_esc,
            "</title>", response_header, "</head>\n<body>\n", NULL);
    print_error(resp, opErrorMsg);
    resp_appendStrL(resp, "<h3><a href=\"/\">", hostname_esc,
            "</a>&emsp;<a href=\"/", share_name, "\">", share_name, "</a>",
            NULL);
    if( strcmp(query_dir, "/") ) {
        resp_appendStr(resp, "&emsp;");
        for(s = query_dir+1; (sn = strchr(s, '/')) != NULL && sn[1]; s = sn+1) {
            resp_appendStrL(resp, "/<a href=\"/", share_name, NULL);
            resp_appendData(resp, query_dir, sn-query_dir);
            resp_appendStr(resp, "\">");
            resp_appendData(resp, s, sn-s);
            resp_appendStr(resp, "</a>");
        }
        resp_appendStrL(resp, "/<a href=\"/", share_name, query_dir,
                "\">", s, "</a>", NULL);
    }
    resp_appendStr(resp, "</h3>\n<table><tbody>\n");
    if( strcmp(query_dir, "/") ) {
        resp_appendStrL(resp, "<tr>\n"
             "<td><span class=\"plusgray\">+</span></td>\n"
             "<td><a style=\"white-space: pre\" href=\"/", share_name, NULL);
        resp_appendData(resp, query_dir, strrchr(query_dir, '/')-query_dir);
        resp_appendStr(resp, "\"> .. </a></td><td></td>\n</tr>\n");
    }
    for(cur_ent = entries; cur_ent->fname; ++cur_ent) {
        resp_appendStrL(resp, "<tr><td onclick=\"showOptions(this)\"><span "
                "class=\"", cur_ent->is_dir ? "plusdir" : "plusfile",
                "\">+</span></td>", NULL);
        fname_esc = escapeHtml(cur_ent->fname, 0);
        resp_appendStrL(resp, "<td><a href=\"/", share_name, query_dir,
                trailsl, fname_esc, "\">", fname_esc, "</a></td>", NULL);
        if( cur_ent->is_dir )
            resp_appendStr(resp, "<td></td>");
        else{
            char buf[20];
            sprintf(buf, "%u", (cur_ent->size+1023) / 1024);
            resp_appendStrL(resp, "<td style=\"text-align: right\">", buf,
                    " kB</td>", NULL);
        }
        resp_appendStr(resp, "</tr>");

        resp_appendStr(resp, "<tr style=\"display: none\"><td></td>\n");
        if( isModifiable ) {
            resp_appendStrL(resp, "<td colspan=\"2\"><form method=\"POST\" "
                    "enctype=\"multipart/form-data\">"
                    "<input type=\"hidden\" name=\"file\" value=\"",
                    fname_esc, "\"/>new name:<select name=\"new_dir\">\n",
                    NULL);
            for(s = query_dir; s != NULL && s[1]; s = strchr(s+1, '/') ) {
                resp_appendStr(resp, "<option>");
                resp_appendData(resp, query_dir, s - query_dir);
                resp_appendStr(resp, "/</option>\n");
            }
            resp_appendStrL(resp, "<option selected>",  query_dir, trailsl,
                    "</option>\n", NULL);
            for(optent = entries; optent->fname; ++optent) {
                if( optent != cur_ent && optent->is_dir ) {
                    resp_appendStrL(resp, "<option>", query_dir, trailsl,
                            escapeHtml(optent->fname, 0), "/</option>\n", NULL);
                }
            }
            resp_appendStrL(resp,
                    "</select><input name=\"new_name\" value=\"", fname_esc,
                    "\"/><input type=\"submit\" name=\"do_rename\" "
                    "value=\"Rename\"/><br/>\n"
                    "<input type=\"submit\" name=\"do_delete\" "
                    "value=\"Delete\" onclick=\"return confirm("
                    "'delete &quot;", escapeHtml(cur_ent->fname, 1),
                    "&quot; ?')\"/>\n"
                    "</form></td>\n", NULL);
        }else{
            resp_appendStr(resp,
                    "<td colspan=\"2\">No action possible</td>\n");
        }
        resp_appendStr(resp, "</tr>\n");
    }
    resp_appendStrL(resp, "</tbody></table>",
            isModifiable ? response_footer : "", "</body></html>\n", NULL);
    return resp;
}

static RespBuf *printShares(int sysErrno, int onlyHead)
{
    char hostname[HOST_NAME_MAX];
    const char *name_esc, *hostname_esc;
    const Share *cur_ent;
    RespBuf *resp;

    gethostname(hostname, sizeof(hostname));
    hostname_esc = escapeHtml(hostname, 0);
    resp = resp_new(sysErrno);
    resp_appendHeader(resp, "Content-Type", "text/html; charset=utf-8");
    if( ! onlyHead ) {
        resp_appendStrL(resp, "<html><head><title>", hostname_esc,
                " shares</title>", response_header, "</head>\n<body>\n", NULL);
        print_error(resp, resp_getErrMessage(resp));
        resp_appendStrL(resp, "<h3><a href=\"/\">", hostname_esc,
                "</a></a>&emsp;shares</a></h3>\n"
                "<table><tbody>\n", NULL);
        for(cur_ent = config_getShares(); cur_ent->name; ++cur_ent) {
            name_esc = escapeHtml(cur_ent->name, 0);
            resp_appendStrL(resp,
                    "<tr><td><span class=\"plusgray\">+</span></td>"
                    "<td><a href=\"", name_esc, "\">", name_esc,
                    "</a></td></tr>", NULL);
        }
        resp_appendStr(resp, "</tbody></table></body></html>\n");
    }
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

static RespBuf *send_file(const char *share_name, const char *rootdir,
        const char *fname, int onlyHead)
{
    FILE *fp;
    char buf[65536];
    int rd;
    char *realpath;
    RespBuf *resp;

    realpath = format_path(rootdir, fname);
    if( (fp = fopen(realpath, "r")) != NULL ) {
        dolog("send_file: opened %s", realpath);
        resp = resp_new(0);
        resp_appendHeader(resp, "Content-Type",
                getContentTypeByFileExt(fname));
        if( ! onlyHead ) {
            // TODO: escape filename
            sprintf(buf, "inline; filename=\"%s\"", strrchr(fname, '/')+1);
            resp_appendHeader(resp, "Content-Disposition", buf);
            while( (rd = fread(buf, 1, sizeof(buf), fp)) > 0 ) {
                resp_appendData(resp, buf, rd);
            }
        }
        fclose(fp);
    }else{
        resp = printErrorPage(errno, share_name, onlyHead);
    }
    free(realpath);
    return resp;
}

static RespBuf *print_response(const char *rootdir, const char *share_name,
        const char *query_file, const char *opErrorMsg, int onlyHead)
{
    struct disp_entry *entries = NULL;
    int errNum, isModifiable;
    RespBuf *resp;

    entries = load_dir(rootdir, query_file, &isModifiable, &errNum);
    if( entries != NULL ) {
        resp = print_dir_contents(entries, isModifiable, share_name,
                query_file, opErrorMsg, onlyHead);
        free_loaded_dir(entries);
    }else if( errNum == ENOTDIR && opErrorMsg == NULL ) {
        resp = send_file(share_name, rootdir, query_file, onlyHead);
    }else{
        resp = printErrorPage(errno, share_name, onlyHead);
    }
    return resp;
}

/* searches for boundary in multipart browser request
*/
static const char *find_boundary(const char *from, unsigned len,
        const char *boundary)
{
    int blen = strlen(boundary);

    while( len >= blen + 2) {
        if( ! memcmp(from, "--", 2) && ! memcmp(from+2, boundary, blen) )
            return from;
        ++from;
        --len;
    }
    return NULL;
}

struct content_part {
    DataChunk name;
    DataChunk filename;
    DataChunk data;
};

static int parse_part(const DataChunk *part, struct content_part *res)
{
    DataChunk contentDisp, name, value;

    dolog("parse part: partlen=%u", part->len);
    res->data = *part;
    dchClear(&contentDisp);
    while( res->data.len >= 2 && ! dchStartsWithStr(&res->data, "\r\n") ) {
        if( dchStartsWithStr(&res->data, "Content-Disposition:") ) {
            if( ! dchExtractTillStr(&res->data, "\r\n", &contentDisp) )
                return 0;
        }else{
            if( ! dchShiftAfterStr(&res->data, "\r\n") )
                return 0;
        }
    }
    if( ! dchShift(&res->data, 2) )
        return 0;
    if( contentDisp.data == NULL ) {
        dolog("no Content-Disposition in part");
        return 0;
    }
    /* Content-Disposition: form-data; name="file"; filename="Test.xml" */
    dchClear(&res->name);
    dchClear(&res->filename);
    while( dchShiftAfterChr(&contentDisp, ';') ) {
        dchSkip(&contentDisp, " ");
        if( ! dchExtractTillStr(&contentDisp, "=", &name) )
            return 0;
        if( ! dchStartsWithStr(&contentDisp, "\"") )
            return 0;
        dchShift(&contentDisp, 1);
        if( ! dchExtractTillStr(&contentDisp, "\"", &value) )
            return 0;
        if( dchEqualsStr(&name, "name") ) {
            res->name = value;
        }else if( dchEqualsStr(&name, "filename") ) {
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


static MemBuf *upload_file(const char *rootdir, const char *dir,
        const DataChunk *dchfname, const DataChunk *data)
{
    FILE *fp;
    char *fname, *fname_real;
    MemBuf *res = NULL;

    fname = dchDupToStr(dchfname);
    dolog("upload_file: adding file=%s len=%u\n", fname, data->len);
    if( dchEqualsStr(dchfname, "") ) {
        res = fmtError(0, "unable to add file with empty name", NULL);
    }else{
        fname_real = format_path3(rootdir, dir, fname);
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

static MemBuf *rename_file(const char *rootdir, const char *old_dir,
        const DataChunk *dch_old_name,
        const DataChunk *dch_new_dir, const DataChunk *dch_new_name)
{
    char *old_name, *new_dir, *new_name, *oldpath, *newpath;
    MemBuf *res = NULL;

    if( dch_old_name->data == NULL || dch_new_dir->data == NULL ||
            dch_new_name->data == NULL )
    {
        res = fmtError(0, "not all parameters provided for rename", NULL);
    }else{
        old_name = dchDupToStr(dch_old_name);
        new_dir = dchDupToStr(dch_new_dir);
        new_name = dchDupToStr(dch_new_name);
        oldpath = format_path3(rootdir, old_dir, old_name);
        newpath = format_path3(rootdir, new_dir, new_name);
        dolog("rename_file: %s -> %s", oldpath, newpath);
        if( rename(oldpath, newpath) != 0 ) {
            res = fmtError(errno, "rename ", old_name, " failed", NULL);
        }
        free(oldpath);
        free(newpath);
        free(new_name);
        free(new_dir);
        free(old_name);
    }
    return res;
}

static MemBuf *create_newdir(const char *rootdir, const char *dir_in,
        const DataChunk *dch_new_dir)
{
    char *path, *new_dir;
    MemBuf *res = NULL;

    if( dch_new_dir->len == 0 || dch_new_dir->data[0] == '\0' ) {
        res = fmtError(0, "unable to create directory with empty name", NULL);
    }else if( dchIndexOfStr(dch_new_dir, "/") >= 0 ) {
        res = fmtError(0, "directory name cannot contain slashes"
                "&emsp;&ndash;&emsp;\"/\"", NULL);
    }else{
        new_dir = dchDupToStr(dch_new_dir);
        path = format_path3(rootdir, dir_in, new_dir);
        dolog("create dir: %s", path);
        if( mkdir(path, S_IRWXU|S_IRWXG|S_IRWXO) != 0 ) {
            res = fmtError(errno, "unable to create directory \"", new_dir,
                    "\"", NULL);
        }
        free(path);
        free(new_dir);
    }
    return res;
}

static MemBuf *delete_file(const char *rootdir, const char *dir,
        const DataChunk *dch_fname)
{
    char *path, *fname;
    struct stat st;
    MemBuf *res = NULL;
    int opRes;

    if( dch_fname->len == 0 || dch_fname->data[0] == '\0' ) {
        res = fmtError(0, "unable to remove file with empty name", NULL);
    }else{
        fname = dchDupToStr(dch_fname);
        path = format_path3(rootdir, dir, fname);
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

static MemBuf *process_post(const char *rootdir,
        const char *query_dir, const char *ct, const MemBuf *requestBody)
{
    DataChunk partData;
    const char *bodyBeg, *beg, *end;
    struct content_part part, file_part, newdir_part, newname_part;
    enum { RT_UNKNOWN, RT_UPLOAD, RT_RENAME, RT_NEWDIR, RT_DELETE }
        request_type = RT_UNKNOWN;
    MemBuf *res = NULL;

    dchClear(&file_part.name);
    dchClear(&newdir_part.name);
    dchClear(&newname_part.name);
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
    dolog("boundary: %s", ct);
    bodyBeg = mb_data(requestBody);
    beg = bodyBeg;
    while((end = find_boundary(beg, mb_dataLen(requestBody)-(beg-bodyBeg), ct))
            != NULL )
    {
        if( end != bodyBeg ) {
            dchSet(&partData, beg, end-beg-2);
            if( parse_part(&partData, &part) ) {
                if( part.filename.data != NULL )
                    dolog("part: {name=%.*s, filename=%.*s, datalen=%u}",
                            part.name.len, part.name.data,
                            part.filename.len, part.filename.data,
                            part.data.len);
                else
                    dolog("part: {name=%.*s, datalen=%u}",
                            part.name.len, part.name.data, part.data.len);
                if( dchEqualsStr(&part.name, "do_upload") ) {
                    request_type = RT_UPLOAD;
                }else if( dchEqualsStr(&part.name, "do_rename") ) {
                    request_type = RT_RENAME;
                }else if( dchEqualsStr(&part.name, "do_newdir") ) {
                    request_type = RT_NEWDIR;
                }else if( dchEqualsStr(&part.name, "do_delete") ) {
                    request_type = RT_DELETE;
                }else if( dchEqualsStr(&part.name, "file") ) {
                    file_part = part;
                }else if( dchEqualsStr(&part.name, "new_dir") ) {
                    newdir_part = part;
                }else if( dchEqualsStr(&part.name, "new_name") ) {
                    newname_part = part;
                }
            }else{
                res = fmtError(0, "malformed form data", NULL);
                goto err;
            }
        }
        beg = end + strlen(ct) + 2;
        if( ! memcmp(beg, "--\r\n", 4) )
            break;
        beg += 2;
    }
    switch( request_type ) {
    case RT_UPLOAD:
        res = upload_file(rootdir, query_dir, &file_part.filename,
                &file_part.data);
        break;
    case RT_RENAME:
        res = rename_file(rootdir, query_dir, &file_part.data,
                &newdir_part.data, &newname_part.data);
        break;
    case RT_NEWDIR:
        res = create_newdir(rootdir, query_dir, &newdir_part.data);
        break;
    case RT_DELETE:
        res = delete_file(rootdir, query_dir, &file_part.data);
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
    const char *queryFile, *slash, *method;
    const Share *curmap = NULL;
    char *shareName;
    RespBuf *resp;

    method = req_getMethod(req);
    isHeadReq = !strcmp(method, "HEAD");
    isPostReq = !strcmp(method, "POST");
    queryFile = req_getPath(req);
    queryFileLen = strlen(queryFile);
    if( queryFileLen >= 3 && (strstr(queryFile, "/../") != NULL ||
            !strcmp(queryFile+queryFileLen-3, "/.."))) 
    {
        resp = printErrorPage(EPERM, "", isHeadReq); /* Forbidden */
    }else if( ! strcmp(queryFile, "/") ) {
        resp = printShares(0, isHeadReq);
    }else{
        ++queryFile;   /* skip initial slash */
        if( (slash = strchr(queryFile, '/')) != NULL ) {
            shareName = malloc(slash-queryFile+1);
            memcpy(shareName, queryFile, slash-queryFile);
            shareName[slash-queryFile] = '\0';
            queryFile = slash;
        }else{
            shareName = strdup(queryFile);
            queryFile = "/";
        }
        for(curmap = config_getShares(); curmap->name != NULL &&
                strcmp(curmap->name, shareName); ++curmap)
            ;
        if( curmap->name != NULL ) {
            MemBuf *opErrBuf = NULL;
            const char *opErrorMsg = NULL;
            if( isPostReq ) {
                opErrBuf = process_post(curmap->rootdir, queryFile,
                        req_getHeaderVal(req, "Content-Type"),
                        req_getBody(req));
            }
            if( opErrBuf != NULL ) {
                mb_appendData(opErrBuf, "", 1);
                opErrorMsg = mb_data(opErrBuf);
            }
            resp = print_response(curmap->rootdir, curmap->name,
                    queryFile, opErrorMsg, isHeadReq);
            mb_free(opErrBuf);
        }else{
            resp = printShares(ENOENT, isHeadReq);
        }
        free(shareName);
    }
    return resp;
}

