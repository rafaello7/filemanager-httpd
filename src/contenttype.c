#include <stdbool.h>
#include "contenttype.h"
#include <string.h>


const char *cttype_getContentTypeByFileExt(const char *fname)
{
    static struct mime_type { const char *ext, *mime; } mime_types[] = {
        { "aac",     "audio/x-hx-aac-adts" },
        { "avi",     "video/x-msvideo" },
        { "bmp",     "image/bmp" },
        { "bz2",     "application/x-bzip2" },
        { "c",       "text/x-c" },
        { "conf",    "text/plain" },
        { "css",     "text/css" },
        { "csv",     "text/csv" },
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
        { "js",      "text/javascript" },
        { "json",    "application/json" },
        { "jsp",     "text/html; charset=utf-8" },
        { "m3u",     "audio/x-mpegurl" },
        { "mid",     "audio/midi" },
        { "midi",    "audio/midi" },
        { "mjs",     "text/javascript" },
        { "mov",     "video/quicktime" },
        { "mp2",     "audio/mpeg" },
        { "mp3",     "audio/mpeg" },
        { "mp4",     "video/mp4" },
        { "mpe",     "video/mpeg" },
        { "mpeg",    "video/mpeg" },
        { "mpg",     "video/mpeg" },
        { "ogg",     "video/ogg" },
        { "pdf",     "application/pdf" },
        { "png",     "image/png" },
        { "ppt",     "application/vnd.ms-powerpoint" },
        { "ps",      "application/postscript" },
        { "qt",      "video/quicktime" },
        { "ra",      "audio/x-realaudio" },
        { "ram",     "audio/x-pn-realaudio" },
        { "rtf",     "text/rtf" },
        { "sh",      "text/plain; charset=utf-8" },
        { "sql",     "application/sql" },
        { "svg",     "image/svg+xml" },
        { "svgz",    "image/svg+xml" },
        { "tar",     "application/x-tar" },
        { "tif",     "image/tiff" },
        { "tiff",    "image/tiff" },
        { "tsv",     "text/tab-separated-values; charset=utf-8" },
        { "txt",     "text/plain; charset=utf-8" },
        { "wasm",    "application/wasm" },
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
    return res;
}

