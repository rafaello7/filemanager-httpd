#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/*#define DEBUG*/

#ifdef DEBUG
static void dolog(const char *fmt, ...)
{
    static FILE *fp = NULL;
    va_list args;

    if( fp == NULL ) {
        fp = fopen("/tmp/filemanager_cgi.log", "a");
        if( fp == NULL )
            fp = fopen("/dev/null", "w");
        if( fp == NULL )
            return;
        fprintf(fp, "-- run --\n");
        fflush(fp);
    }
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fprintf(fp, "\n");
    fflush(fp);
}
#else
#define dolog(...)
#endif

static char errbuf[10000];

static void printerr(int errNo, const char *fmt, ...)
{
    va_list args;
    char *s = errbuf;

    if( errbuf[0] ) {
        s = errbuf + strlen(errbuf);
        s += sprintf(s, "; ");
    }
    va_start(args, fmt);
    s += vsprintf(s, fmt, args);
    va_end(args);
    if( errNo != 0 ) {
        sprintf(s, ": %s", strerror(errNo));
    }
}

struct directory_map {
    char *name;
    char *rootdir;
};

static struct directory_map *parse_config(void)
{
    FILE *fp;
    char buf[1024];
    char *val;
    struct directory_map *dirmaps = NULL;
    int count = 0;
    static const char config_fname[] = "/etc/filemanager-cgi.conf";

    if( (fp = fopen(config_fname, "r")) != NULL ) {
        while( fgets(buf, sizeof(buf), fp) != NULL ) {
            if( buf[0] == '\0' || buf[0] == '#' )
                continue;
            buf[strlen(buf)-1] = '\0';
            if( (val = strchr(buf, '=')) != NULL ) {
                *val++ = '\0';
                dirmaps = realloc(dirmaps,
                        (count+1) * sizeof(struct directory_map));
                dirmaps[count].name = strdup(buf);
                dirmaps[count].rootdir = strdup(val);
                ++count;
            }
        }
    }else{
        printerr(errno, "unable to open configuration file %s", config_fname);
    }
    dirmaps = realloc(dirmaps, (count+1) * sizeof(struct directory_map));
    dirmaps[count].name = NULL;
    dirmaps[count].rootdir = NULL;
    return dirmaps;
}

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

#ifdef DEBUG
static void dumpenv(void)
{
    extern char **environ;
    int i;

    for(i = 0; environ[i]; ++i) {
        dolog("env: %s", environ[i]);
    }
}
#else
#define dumpenv()
#endif

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
        int *is_modifiable)
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
        *is_modifiable = access(dname_real, W_OK) == 0;
    }else{
        printerr(errno, "unable to read directory %s", dname);
        *is_modifiable = 0;
    }
    free(dname_real);
    res = realloc(res, (count+1) * sizeof(struct disp_entry));
    res[count].fname = NULL;
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

static const char *unescapeUrl(const char *s)
{
    int len;
    char *res, *perc;
    char num[3];

    if( (perc = strchr(s, '%')) == NULL )
        return s;
    len = perc - s;
    res = malloc( strlen(s) );
    memcpy(res, s, len);
    s += len;
    while( *s ) {
        if( *s == '%' ) {
            ++s;
            num[0] = *s;
            if( *s ) {
                ++s;
                num[1] = *s;
                if( *s ) ++s;
            }
            num[2] = '\0';
            ((unsigned char*)res)[len++] = strtoul(num, NULL, 16);
        }else{
            res[len++] = *s++;
        }
    }
    res[len] = '\0';
    return res;
}

static void print_error(void)
{
    if( errbuf[0] ) {
        printf("<h3 style=\"text-align: center; background-color: yellow\">"
                "%s</h3>\n", escapeHtml(errbuf, 0));
    }
}

static void print_error_page(const char *share_name)
{
    char hostname[HOST_NAME_MAX];

    gethostname(hostname, sizeof(hostname));
    printf("Content-type: text/html; charset=utf-8\r\n");
    printf("\r\n");
    printf("<html><head><title>%s on %s</title></head><body>\n",
            escapeHtml(share_name, 0), escapeHtml(hostname, 0));
    print_error();
    printf("</body></html>\n");
}

static void print_dir_contents(const char *rootdir, const char *share_name,
        const char *query_dir)
{
    char hostname[HOST_NAME_MAX];
    const char *s, *sn, *trailsl, *hostname_esc, *fname_esc;
    struct disp_entry *entries, *cur_ent, *optent;
    int is_modifiable;

    entries = load_dir(rootdir, query_dir, &is_modifiable);
    trailsl = query_dir[strlen(query_dir)-1] == '/' ? "" : "/";
    gethostname(hostname, sizeof(hostname));
    share_name = escapeHtml(share_name, 0);
    query_dir = escapeHtml(query_dir, 0);
    hostname_esc = escapeHtml(hostname, 0);
    printf("Content-type: text/html; charset=utf-8\r\n");
    printf("\r\n");
    printf("<html><head><title>%s on %s</title>%s</head>\n",
            share_name[0] ? share_name : query_dir, hostname_esc,
            response_header);
    printf("<body>\n");
    print_error();
    printf("<h3><a href=\"?\">%s</a>&emsp;<a href=\"?%s\">%s</a>",
            hostname_esc, share_name, share_name);
    if( strcmp(query_dir, "/") ) {
        printf("&emsp;");
        for(s = query_dir+1; (sn = strchr(s, '/')) != NULL && sn[1]; s = sn+1) {
            printf("/<a href=\"?%s%.*s\">%.*s</a>", share_name,
                    (int)(sn-query_dir), query_dir, (int)(sn-s), s);
        }
        printf("/<a href=\"?%s%s\">%s</a>", share_name, query_dir, s);
    }
    printf("</h3>\n");
    printf("<table><tbody>\n");
    if( strcmp(query_dir, "/") ) {
        printf("<tr>\n"
         "<td><span class=\"plusgray\">+</span></td>\n"
         "<td><a style=\"white-space: pre\" "
                "href=\"?%s%.*s\"> .. </a></td><td></td>\n"
         "</tr>\n", share_name, (int)(strrchr(query_dir, '/')-query_dir),
         query_dir);
    }
    for(cur_ent = entries; cur_ent->fname; ++cur_ent) {
        printf("<tr><td onclick=\"showOptions(this)\"><span "
                "class=\"%s\">+</span></td>",
                cur_ent->is_dir ? "plusdir" : "plusfile");
        fname_esc = escapeHtml(cur_ent->fname, 0);
        printf("<td><a href=\"?%s%s%s%s\">%s</a></td>",
                share_name, query_dir, trailsl, fname_esc, fname_esc);
        if( cur_ent->is_dir )
            printf("<td></td>");
        else
            printf("<td style=\"text-align: right\">%u kB</td>",
                    (cur_ent->size+1023) / 1024);
        printf("</tr>");

        printf("<tr style=\"display: none\"><td></td>\n");
        if( is_modifiable ) {
            printf("<td colspan=\"2\"><form method=\"POST\" "
                    "enctype=\"multipart/form-data\">"
                    "<input type=\"hidden\" name=\"file\" value=\"%s\"/>"
                    "new name:<select name=\"new_dir\">\n", fname_esc);
            for(s = query_dir; s != NULL && s[1]; s = strchr(s+1, '/') ) {
                printf("<option>%.*s/</option>\n",
                        (int)(s - query_dir), query_dir);
            }
            printf("<option selected>%s%s</option>\n", query_dir, trailsl);
            for(optent = entries; optent->fname; ++optent) {
                if( optent != cur_ent && optent->is_dir ) {
                    printf("<option>%s%s%s/</option>\n", query_dir, trailsl,
                            escapeHtml(optent->fname, 0));
                }
            }
            printf("</select><input name=\"new_name\" value=\"%s\"/>",
                    fname_esc);
            printf("<input type=\"submit\" name=\"do_rename\" "
                    "value=\"Rename\"/><br/>\n");
            printf("<input type=\"submit\" name=\"do_delete\" "
                    "value=\"Delete\" onclick=\"return confirm("
                    "'delete &quot;%s&quot; ?')\"/>\n",
                        escapeHtml(cur_ent->fname, 1));
            printf("</form></td>\n");
        }else{
            printf("<td colspan=\"2\">No action possible</td>\n");
        }
        printf("</tr>\n");
    }
    printf("</tbody></table>%s</body></html>\n",
            is_modifiable ? response_footer : "");
    free_loaded_dir(entries);
}

static void print_shares(struct directory_map *maps)
{
    char hostname[HOST_NAME_MAX];
    const char *name_esc, *hostname_esc;
    struct directory_map *cur_ent;

    gethostname(hostname, sizeof(hostname));
    hostname_esc = escapeHtml(hostname, 0);
    printf("Content-type: text/html; charset=utf-8\r\n");
    printf("\r\n");
    printf("<html><head><title>%s shares</title>%s</head>\n",
            hostname_esc, response_header);
    printf("<body>\n");
    print_error();
    printf("<h3><a href=\"?\">%s</a></a>&emsp;shares</a></h3>\n",
            hostname_esc);
    printf("<table><tbody>\n");
    for(cur_ent = maps; cur_ent->name; ++cur_ent) {
        name_esc = escapeHtml(cur_ent->name, 0);
        printf("<tr><td><span class=\"plusgray\">+</span></td>");
        printf("<td><a href=\"?%s\">%s</a></td></tr>", name_esc, name_esc);
    }
    printf("</tbody></table></body></html>\n");
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

static void send_file(const char *share_name, const char *rootdir,
        const char *fname, int size)
{
    FILE *fp;
    char buf[16384];
    int rd;
    char *realpath;

    realpath = format_path(rootdir, fname);
    if( (fp = fopen(realpath, "r")) != NULL ) {
        dolog("send_file: opened %s", realpath);
        printf("Content-type: %s\r\n", getContentTypeByFileExt(fname));
        printf("Content-Disposition: inline; filename=\"%s\"\r\n",
                strrchr(fname, '/')+1); // TODO: escape filename
        printf("Content-length: %d\r\n\r\n", size);
        while( (rd = fread(buf, 1, sizeof(buf), fp)) > 0 ) {
            fwrite(buf, 1, rd, stdout);
        }
        fclose(fp);
    }else{
        printerr(errno, "unable to open %s", fname);
        print_error_page(share_name);
    }
    free(realpath);
}

static void print_response(const char *rootdir, const char *share_name,
        const char *query_file)
{
    struct stat st;
    int is_dir = 0;
    char *realpath;

    realpath = format_path(rootdir, query_file);
    dolog("response: path=%s", realpath);
    if( stat(realpath, &st) == 0 ) {
        if( S_ISDIR(st.st_mode) )
            is_dir = 1;
    }else{
        printerr(errno, "%s", query_file);
    }
    free(realpath);
    if( is_dir ) {
        print_dir_contents(rootdir, share_name, query_file);
    }else if( errbuf[0] ) {
        print_error_page(share_name);
    }else{
        send_file(share_name, rootdir, query_file, st.st_size);
    }
}

/* searches for boundary in multipart browser request
*/
static char *find_boundary(char *from, int len,
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
    const char *name;
    const char *filename;
    const char *data;
    int datalen;
};

static int parse_part(char *part, int partlen, struct content_part *res)
{
    char *content_disp = NULL, *valbeg, *valend;

    dolog("parse part: partlen=%d", partlen);
    if( partlen < 2 )
        return 0;
    while( strncmp(part, "\r\n", 2) ) {
        if( partlen > 20 && ! strncmp(part, "Content-Disposition:", 20) ) {
            content_disp = part;
        }
        while( partlen >= 4 && strncmp(part, "\r\n", 2) ) {
            ++part;
            --partlen;
        }
        if( partlen < 4 )
            return 0;
        part += 2;
        partlen -= 2;
    }
    if( content_disp == NULL ) {
        dolog("no Content-Disposition in part");
        return 0;
    }
    // Content-Disposition: form-data; name="file"; filename="Test.xml"
    res->name = NULL;
    res->filename = NULL;
    part[partlen] = '\0';   // for controls returning simple string 
    res->data = part + 2;
    res->datalen = partlen - 2;
    *strstr(content_disp, "\r\n") = '\0';
    while( (content_disp = strchr(content_disp, ';')) != NULL ) {
        ++content_disp;
        while( *content_disp == ' ' )
            ++content_disp;
        if( (valbeg = strchr(content_disp, '=')) == NULL )
            return 0;
        *valbeg++ = '\0';
        if( *valbeg++ != '"' )
            return 0;
        if( (valend = strchr(valbeg, '"')) == NULL )
            return 0;
        *valend = '\0';
        if( !strcmp(content_disp, "name") )
            res->name = valbeg;
        else if( !strcmp(content_disp, "filename") )
            res->filename = valbeg;
        content_disp = valend + 1;
    }
    return res->name != NULL;
}

static void upload_file(const char *rootdir, const char *dir,
        const char *fname, const char *data, int datalen)
{
    FILE *fp;
    char *fname_real;

    dolog("upload_file: adding file=%s\n", fname);
    if( !strcmp(fname, "") ) {
        printerr(0, "unable to add file with empty name");
        return;
    }
    fname_real = format_path3(rootdir, dir, fname);
    if( access(fname_real, F_OK) == 0 ) {
        printerr(0, "file %s already exists", fname);
    }else{
        if( (fp = fopen(fname_real, "w")) == NULL ) {
            printerr(errno, "unable to open %s for writing", fname);
        }else{
            if( fwrite(data, datalen, 1, fp) != 1 ) {
                printerr(errno, "error during file write");
                unlink(fname_real);
            }
            fclose(fp);
        }
    }
    free(fname_real);
}

static void rename_file(const char *rootdir, const char *old_dir,
        const char *old_name,
        const char *new_dir, const char *new_name)
{
    char *oldpath, *newpath;

    if( old_name == NULL || new_dir == NULL || new_name == NULL ) {
        printerr(0, "not all parameters provided for rename");
        return;
    }
    oldpath = format_path3(rootdir, old_dir, old_name);
    newpath = format_path3(rootdir, new_dir, new_name);
    dolog("rename_file: %s -> %s", oldpath, newpath);
    if( rename(oldpath, newpath) != 0 ) {
        printerr(errno, "unable to rename %s", old_name);
    }
    free(oldpath);
    free(newpath);
}

static void create_newdir(const char *rootdir, const char *dir_in,
        const char *new_dir)
{
    char *path;

    if( new_dir == NULL || new_dir[0] == '\0' ) {
        printerr(0, "unable to create directory with empty name");
        return;
    }
    if( strchr(new_dir, '/') != NULL ) {
        printerr(0, "directory name cannot contain slashes"
                "&emsp;&ndash;&emsp;\"/\"");
        return;
    }
    path = format_path3(rootdir, dir_in, new_dir);
    dolog("create dir: %s", path);
    if( mkdir(path, S_IRWXU|S_IRWXG|S_IRWXO) != 0 ) {
        printerr(errno, "unable to create directory \"%s\"", new_dir);
    }
    free(path);
}

static void delete_file(const char *rootdir, const char *dir, const char *fname)
{
    char *path;
    struct stat st;
    int res;

    if( fname == NULL || fname[0] == '\0' ) {
        printerr(0, "unable to remove file with empty name");
        return;
    }
    path = format_path3(rootdir, dir, fname);
    dolog("delete: %s", path);
    if( stat(path, &st) == 0 ) {
        if( S_ISDIR(st.st_mode) ) {
            res = rmdir(path);
        }else{
            res = unlink(path);
        }
        if( res != 0 ) {
            printerr(errno, "unable to delete %s", fname);
        }
    }else{
        printerr(errno, "%s", fname);
    }
    free(path);
}

static void process_post(const char *rootdir,
        const char *query_dir)
{
    char *buf = NULL, *bpos, *beg, *end;
    const char *ct;
    int rd, len;
    struct content_part part, file_part = { .name = NULL };
    struct content_part newdir_part = { .name = NULL };
    struct content_part newname_part = { .name = NULL };
    enum { RT_UNKNOWN, RT_UPLOAD, RT_RENAME, RT_NEWDIR, RT_DELETE }
        request_type = RT_UNKNOWN;

    len = atoi(getenv("CONTENT_LENGTH"));
    if( len == 0 ) {
        printerr(0, "bad content length: %s", getenv("CONTENT_LENGTH"));
        goto err;
    }
    bpos = buf = malloc(len);
    while( bpos - buf < len ) {
        if( (rd = read(0, bpos, len - (bpos-buf))) < 0 ) {
            printerr(errno, "input read fail");
            goto err;
        }
        bpos += rd;
    }
    ct = getenv("CONTENT_TYPE");
    if( memcmp(ct, "multipart/form-data; boundary=", 30) ) {
        printerr(0, "bad content type: %s", ct);
        goto err;
    }
    ct += 30;
    dolog("boundary: %s", ct);
    beg = buf;
    while( (end = find_boundary(beg, len - (beg-buf), ct)) != NULL ) {
        if( end != buf ) {
            if( parse_part(beg, end-beg-2, &part) ) {
                if( part.filename != NULL )
                    dolog("part: {name=%s, filename=%s, datalen=%d}",
                            part.name, part.filename, part.datalen);
                else
                    dolog("part: {name=%s, datalen=%d}",
                            part.name, part.datalen);
                if( ! strcmp(part.name, "do_upload") ) {
                    request_type = RT_UPLOAD;
                }else if( ! strcmp(part.name, "do_rename") ) {
                    request_type = RT_RENAME;
                }else if( ! strcmp(part.name, "do_newdir") ) {
                    request_type = RT_NEWDIR;
                }else if( ! strcmp(part.name, "do_delete") ) {
                    request_type = RT_DELETE;
                }else if( ! strcmp(part.name, "file") ) {
                    file_part = part;
                }else if( ! strcmp(part.name, "new_dir") ) {
                    newdir_part = part;
                }else if( ! strcmp(part.name, "new_name") ) {
                    newname_part = part;
                }
            }else{
                printerr(0, "malformed form data");
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
        upload_file(rootdir, query_dir, file_part.filename,
                file_part.data, file_part.datalen);
        break;
    case RT_RENAME:
        rename_file(rootdir, query_dir, file_part.data, newdir_part.data,
                newname_part.data);
        break;
    case RT_NEWDIR:
        create_newdir(rootdir, query_dir, newdir_part.data);
        break;
    case RT_DELETE:
        delete_file(rootdir, query_dir, file_part.data);
        break;
    default:
        printerr(0, "unrecognized request");
        break;
    }
err:
    free(buf);
}

int main(int argc, char *argv[])
{
    int query_file_len;
    const char *request_method, *query_file, *slash;
    struct directory_map *maps, *curmap = NULL;
    char *mapkey;

    dumpenv();
    query_file = getenv("QUERY_STRING");
    if( query_file == NULL ) {
        query_file = "";
    }else if( strchr(query_file, '%') != NULL ) {
        query_file = unescapeUrl(query_file);
    }
    if( (request_method = getenv("REQUEST_METHOD")) == NULL )
        request_method = "GET";
    maps = parse_config();
    if( (slash = strchr(query_file, '/')) != NULL ) {
        mapkey = malloc(slash-query_file+1);
        memcpy(mapkey, query_file, slash-query_file);
        mapkey[slash-query_file] = '\0';
        query_file = slash;
    }else{
        mapkey = strdup(query_file);
        query_file = "/";
    }
    query_file_len = strlen(query_file);
    if( query_file_len >= 3 && (strstr(query_file, "/../") != NULL ||
            !strcmp(query_file+query_file_len-3, "/.."))) 
    {
        printerr(0, "forbidden");
        print_error_page("");
    }else{
        for(curmap = maps; curmap->name != NULL && strcmp(curmap->name, mapkey);
                ++curmap)
            ;
        if( curmap->name != NULL ) {
            if( ! strcmp(request_method, "POST") ) {
                process_post(curmap->rootdir, query_file);
            }
            print_response(curmap->rootdir, curmap->name, query_file);
        }else{
            if( ! strcmp(request_method, "POST") || mapkey[0] ||
                    strcmp(query_file, "/"))
            {
                printerr(0, "share with name \"%s\" does not exist", mapkey);
            }
            print_shares(maps);
        }
    }
    free(mapkey);
    return 0;
}

