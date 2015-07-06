/* Copyright (C) 2007-2009 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#include "android/utils/path.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#ifdef _WIN32
#include <process.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <limits.h>
#include <winbase.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#endif

#include "android/utils/debug.h"
#define  D(...)  VERBOSE_PRINT(init,__VA_ARGS__)

#ifndef CHECKED
#  ifdef _WIN32
#    define   CHECKED(ret, call)    (ret) = (call)
#  else
#    define   CHECKED(ret, call)    do { (ret) = (call); } while ((ret) < 0 && errno == EINTR)
#  endif
#endif

/** PATH HANDLING ROUTINES
 **
 **  path_parent() can be used to return the n-level parent of a given directory
 **  this understands . and .. when encountered in the input path
 **/

static __inline__ int
ispathsep(int  c)
{
#ifdef _WIN32
    return (c == '/' || c == '\\');
#else
    return (c == '/');
#endif
}

char*
path_parent( const char*  path, int  levels )
{
    const char*  end = path + strlen(path);
    char*        result;

    while (levels > 0) {
        const char*  base;

        /* trim any trailing path separator */
        while (end > path && ispathsep(end[-1]))
            end--;

        base = end;
        while (base > path && !ispathsep(base[-1]))
            base--;

        if (base <= path) {
            if (end == base+1 && base[0] == '.' && levels == 1)
                return strdup("..");
          /* we can't go that far */
            return NULL;
        }

        if (end == base+1 && base[0] == '.')
            goto Next;

        if (end == base+2 && base[0] == '.' && base[1] == '.') {
            levels += 1;
            goto Next;
        }

        levels -= 1;

    Next:
        end = base - 1;
    }
    result = malloc( end-path+1 );
    if (result != NULL) {
        memcpy( result, path, end-path );
        result[end-path] = 0;
    }
    return result;
}

static char*
substring_dup( const char*  start, const char*  end )
{
    int    len    = end - start;
    char*  result = android_alloc(len+1);
    memcpy(result, start, len);
    result[len] = 0;
    return result;
}

int
path_split( const char*  path, char* *pdirname, char* *pbasename )
{
    const char*  end = path + strlen(path);
    const char*  last;
    char*        basename;

    /* prepare for errors */
    if (pdirname)
        *pdirname = NULL;
    if (pbasename)
        *pbasename = NULL;

    /* handle empty path case */
    if (end == path) {
        return -1;
    }

    /* strip trailing path separators */
    while (end > path && ispathsep(end[-1]))
        end -= 1;

    /* handle "/" and degenerate cases like "////" */
    if (end == path) {
        return -1;
    }

    /* find last separator */
    last = end;
    while (last > path && !ispathsep(last[-1]))
        last -= 1;

    /* handle cases where there is no path separator */
    if (last == path) {
        if (pdirname)
            *pdirname  = ASTRDUP(".");
        if (pbasename)
            *pbasename = substring_dup(path,end);
        return 0;
    }

    /* handle "/foo" */
    if (last == path+1) {
        if (pdirname)
            *pdirname  = ASTRDUP("/");
        if (pbasename)
            *pbasename = substring_dup(path+1,end);
        return 0;
    }

    /* compute basename */
    basename = substring_dup(last,end);
    if (strcmp(basename, ".") == 0 || strcmp(basename, "..") == 0) {
        AFREE(basename);
        return -1;
    }

    if (pbasename)
        *pbasename = basename;
    else {
        AFREE(basename);
    }

    /* compute dirname */
    if (pdirname != NULL)
        *pdirname = substring_dup(path,last-1);

    return 0;
}

char*
path_basename( const char*  path )
{
    char*  basename;

    if (path_split(path, NULL, &basename) < 0)
        return NULL;

    return basename;
}

char*
path_dirname( const char*  path )
{
    char*  dirname;

    if (path_split(path, &dirname, NULL) < 0)
        return NULL;

    return dirname;
}





/** MISC FILE AND DIRECTORY HANDLING
 **/

ABool
path_exists( const char*  path )
{
    int  ret;
    if (path == NULL)
        return 0;
    CHECKED(ret, access(path, F_OK));
    return (ret == 0) || (errno != ENOENT);
}

/* checks that a path points to a regular file */
ABool
path_is_regular( const char*  path )
{
    int          ret;
    struct stat  st;

    if (path == NULL)
        return 0;
    CHECKED(ret, stat(path, &st));
    if (ret < 0)
        return 0;

    return S_ISREG(st.st_mode);
}


/* checks that a path points to a directory */
ABool
path_is_dir( const char*  path )
{
    int          ret;
    struct stat  st;

    if (path == NULL)
        return 0;
    CHECKED(ret, stat(path, &st));
    if (ret < 0)
        return 0;

    return S_ISDIR(st.st_mode);
}

/* checks that one can read/write a given (regular) file */
ABool
path_can_read( const char*  path )
{
    int  ret;
    if (path == NULL)
        return 0;
    CHECKED(ret, access(path, R_OK));
    return (ret == 0);
}

ABool
path_can_write( const char*  path )
{
    int  ret;
    if (path == NULL)
        return 0;
    CHECKED(ret, access(path, R_OK));
    return (ret == 0);
}

ABool
path_can_exec( const char* path )
{
    int  ret;
    if (path == NULL)
        return 0;
    CHECKED(ret, access(path, X_OK));
    return (ret == 0);
}

/* try to make a directory. returns 0 on success, -1 on failure
 * (error code in errno) */
APosixStatus
path_mkdir( const char*  path, int  mode )
{
#ifdef _WIN32
    (void)mode;
    return _mkdir(path);
#else
    int  ret;
    CHECKED(ret, mkdir(path, mode));
    return ret;
#endif
}

static APosixStatus
path_mkdir_recursive( char*  path, unsigned  len, int  mode )
{
    char      old_c;
    int       ret;
    unsigned  len2;

    /* get rid of trailing separators */
    while (len > 0 && ispathsep(path[len-1]))
        len -= 1;

    if (len == 0) {
        errno = ENOENT;
        return -1;
    }

    /* check that the parent exists, 'len2' is the length of
     * the parent part of the path */
    len2 = len-1;
    while (len2 > 0 && !ispathsep(path[len2-1]))
        len2 -= 1;

    if (len2 > 0) {
        old_c      = path[len2];
        path[len2] = 0;
        ret        = 0;
        if ( !path_exists(path) ) {
            /* the parent doesn't exist, so try to create it */
            ret = path_mkdir_recursive( path, len2, mode );
        }
        path[len2] = old_c;

        if (ret < 0)
            return ret;
    }

    /* at this point, we now the parent exists */
    old_c     = path[len];
    path[len] = 0;
    ret       = path_mkdir( path, mode );
    path[len] = old_c;

    return ret;
}

/* ensure that a given directory exists, create it if not,
   0 on success, -1 on failure (error code in errno) */
APosixStatus
path_mkdir_if_needed( const char*  path, int  mode )
{
    int  ret = 0;

    if (!path_exists(path)) {
        ret = path_mkdir(path, mode);

        if (ret < 0 && errno == ENOENT) {
            char      temp[MAX_PATH];
            unsigned  len = (unsigned)strlen(path);

            if (len > sizeof(temp)-1) {
                errno = EINVAL;
                return -1;
            }
            memcpy( temp, path, len );
            temp[len] = 0;

            return path_mkdir_recursive(temp, len, mode);
        }
    }
    return ret;
}

/* return the size of a given file in '*psize'. returns 0 on
 * success, -1 on failure (error code in errno) */
APosixStatus
path_get_size( const char*  path, uint64_t  *psize )
{
#ifdef _WIN32
    /* avoid _stat64 which is only defined in MSVCRT.DLL, not CRTDLL.DLL */
    /* do not use OpenFile() because it has strange search behaviour that could */
    /* result in getting the size of a different file */
    LARGE_INTEGER  size;
    HANDLE  file = CreateFile( /* lpFilename */        path,
                               /* dwDesiredAccess */   GENERIC_READ,
                               /* dwSharedMode */     FILE_SHARE_READ|FILE_SHARE_WRITE,
                               /* lpSecurityAttributes */  NULL,
                               /* dwCreationDisposition */ OPEN_EXISTING,
                               /* dwFlagsAndAttributes */  0,
                               /* hTemplateFile */      NULL );
    if (file == INVALID_HANDLE_VALUE) {
        /* ok, just to play fair */
        errno = ENOENT;
        return -1;
    }
    if (!GetFileSizeEx(file, &size)) {
        /* maybe we tried to get the size of a pipe or something like that ? */
        *psize = 0;
    }
    else {
        *psize = (uint64_t) size.QuadPart;
    }
    CloseHandle(file);
    return 0;
#else
    int    ret;
    struct stat  st;

    CHECKED(ret, stat(path, &st));
    if (ret == 0) {
        *psize = (uint64_t) st.st_size;
    }
    return ret;
#endif
}


ABool
path_is_absolute( const char*  path )
{
#ifdef _WIN32
    if (path == NULL)
        return 0;

    if (path[0] == '/' || path[0] == '\\')
        return 1;

    /* 'C:' is always considered to be absolute
     * even if used with a relative path like C:foo which
     * is different from C:\foo
     */
    if (path[0] != 0 && path[1] == ':')
        return 1;

    return 0;
#else
    return (path != NULL && path[0] == '/');
#endif
}

char*
path_get_absolute( const char* path )
{
    if (path_is_absolute(path)) {
        return ASTRDUP(path);
    }

#ifdef _WIN32
    {
        char* result;
        int   pathLen    = strlen(path);
        int   currentLen = GetCurrentDirectory(0, NULL);

        if (currentLen <= 0) {
            /* Could not get size of working directory. something is
             * really fishy here, return a simple copy */
            return ASTRDUP(path);
        }
        result = malloc(currentLen + pathLen + 2);

        GetCurrentDirectory(currentLen+1, result);
        if (currentLen == 0 || result[currentLen-1] != '\\') {
            result[currentLen++] = '\\';
        }
        memcpy(result + currentLen, path, pathLen+1);

        return result;
    }
#else
    {
        int   pathLen    = strlen(path);
        char  currentDir[PATH_MAX];
        int   currentLen;
        char* result;

        if (getcwd(currentDir, sizeof(currentDir)) == NULL) {
            /* Could not get the current working directory. something is really
            * fishy here, so don't do anything and return a copy */
            return ASTRDUP(path);
        }

        /* Make a new path with <current-path>/<path> */
        currentLen = strlen(currentDir);
        result     = malloc(currentLen + pathLen + 2);

        memcpy(result, currentDir, currentLen);
        if (currentLen == 0 || result[currentLen-1] != '/') {
            result[currentLen++] = '/';
        }
        memcpy(result + currentLen, path, pathLen+1);

        return result;
    }
#endif
}

/** OTHER FILE UTILITIES
 **
 **  path_empty_file() creates an empty file at a given path location.
 **  if the file already exists, it is truncated without warning
 **
 **  path_copy_file() copies one file into another.
 **
 **  both functions return 0 on success, and -1 on error
 **/

APosixStatus
path_empty_file( const char*  path )
{
#ifdef _WIN32
    int  fd = _creat( path, S_IWRITE );
#else
    /* on Unix, only allow the owner to read/write, since the file *
     * may contain some personal data we don't want to see exposed */
    int  fd = creat(path, S_IRUSR | S_IWUSR);
#endif
    if (fd >= 0) {
        close(fd);
        return 0;
    }
    return -1;
}

APosixStatus
path_copy_file( const char*  dest, const char*  source )
{
    int  fd, fs, result = -1;

    /* if the destination doesn't exist, create it */
    if ( access(source, F_OK)  < 0 ||
         path_empty_file(dest) < 0) {
        return -1;
    }

    if ( access(source, R_OK) < 0 ) {
        D("%s: source file is un-readable: %s\n",
          __FUNCTION__, source);
        return -1;
    }

#ifdef _WIN32
    fd = _open(dest, _O_RDWR | _O_BINARY);
    fs = _open(source, _O_RDONLY |  _O_BINARY);
#else
    fd = creat(dest, S_IRUSR | S_IWUSR);
    fs = open(source, S_IREAD);
#endif
    if (fs >= 0 && fd >= 0) {
        char buf[4096];
        ssize_t total = 0;
        ssize_t n;
        result = 0; /* success */
        while ((n = read(fs, buf, 4096)) > 0) {
            if (write(fd, buf, n) != n) {
                /* write failed. Make it return -1 so that an
                 * empty file be created. */
                D("Failed to copy '%s' to '%s': %s (%d)",
                       source, dest, strerror(errno), errno);
                result = -1;
                break;
            }
            total += n;
        }
    }

    if (fs >= 0) {
        close(fs);
    }
    if (fd >= 0) {
        close(fd);
    }
    return result;
}


APosixStatus
path_delete_file( const char*  path )
{
#ifdef _WIN32
    int  ret = _unlink( path );
    if (ret == -1 && errno == EACCES) {
        /* a first call to _unlink will fail if the file is set read-only */
        /* we can however try to change its mode first and call unlink    */
        /* again...                                                       */
        ret = _chmod( path, _S_IREAD | _S_IWRITE );
        if (ret == 0)
            ret = _unlink( path );
    }
    return ret;
#else
    return  unlink(path);
#endif
}


void*
path_load_file(const char *fn, size_t  *pSize)
{
    char*  data;
    int    sz;
    int    fd;

    if (pSize)
        *pSize = 0;

    data   = NULL;

    fd = open(fn, O_BINARY | O_RDONLY);
    if(fd < 0) return NULL;

    do {
        sz = lseek(fd, 0, SEEK_END);
        if(sz < 0) break;

        if (pSize)
            *pSize = (size_t) sz;

        if (lseek(fd, 0, SEEK_SET) != 0)
            break;

        data = (char*) malloc(sz + 1);
        if(data == NULL) break;

        if (read(fd, data, sz) != sz)
            break;

        close(fd);
        data[sz] = 0;

        return data;
    } while (0);

    close(fd);

    if(data != NULL)
        free(data);

    return NULL;
}

#ifdef _WIN32
#  define DIR_SEP  ';'
#else
#  define DIR_SEP  ':'
#endif

char*
path_search_exec( const char* filename )
{
    const char* sysPath = getenv("PATH");
    char        temp[PATH_MAX];
    int         count;
    int         slen;
    const char* p;

    /* If the file contains a directory separator, don't search */
#ifdef _WIN32
    if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
#else
    if (strchr(filename, '/') != NULL) {
#endif
        if (path_exists(filename)) {
            return strdup(filename);
        } else {
            return NULL;
        }
    }

    /* If system path is empty, don't search */
    if (sysPath == NULL || sysPath[0] == '\0') {
        return NULL;
    }

    /* Count the number of non-empty items in the system path
     * Items are separated by DIR_SEP, and two successive separators
     * correspond to an empty item that will be ignored.
     * Also compute the required string storage length. */
    count   = 0;
    slen    = 0;
    p       = sysPath;

    while (*p) {
        char* p2 = strchr(p, DIR_SEP);
        int   len;
        if (p2 == NULL) {
            len = strlen(p);
        } else {
            len = p2 - p;
        }

        do {
            if (len <= 0)
                break;

            snprintf(temp, sizeof(temp), "%.*s/%s", len, p, filename);

            if (path_exists(temp) && path_can_exec(temp)) {
                return strdup(temp);
            }

        } while (0);

        p += len;
        if (*p == DIR_SEP)
            p++;
    }

    /* Nothing, really */
    return NULL;
}
