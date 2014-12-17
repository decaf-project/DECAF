/* Copyright (C) 2007-2008 The Android Open Source Project
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

#include "android/utils/tempfile.h"
#include "android/utils/bufprint.h"
#include "android/utils/debug.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#define  D(...)  ((void)0)

/** TEMP FILE SUPPORT
 **
 ** simple interface to create an empty temporary file on the system.
 **
 ** create the file with tempfile_create(), which returns a reference to a TempFile
 ** object, or NULL if your system is so weird it doesn't have a temporary directory.
 **
 ** you can then call tempfile_path() to retrieve the TempFile's real path to open
 ** it. the returned path is owned by the TempFile object and should not be freed.
 **
 ** all temporary files are destroyed when the program quits, unless you explicitely
 ** close them before that with tempfile_close()
 **/

struct TempFile
{
    const char*  name;
    TempFile*    next;
};

static void       tempfile_atexit();
static TempFile*  _all_tempfiles;

TempFile*
tempfile_create( void )
{
    TempFile*    tempfile;
    const char*  tempname = NULL;

#ifdef _WIN32
    char  temp_namebuff[MAX_PATH];
    char  temp_dir[MAX_PATH];
    char  *p = temp_dir, *end = p + sizeof(temp_dir);
    UINT  retval;

    p = bufprint_temp_dir( p, end );
    if (p >= end) {
        D( "TEMP directory path is too long" );
        return NULL;
    }

    retval = GetTempFileName(temp_dir, "TMP", 0, temp_namebuff);
    if (retval == 0) {
        D( "can't create temporary file in '%s'", temp_dir );
        return NULL;
    }

    tempname = temp_namebuff;
#else
#define  TEMPLATE  "/tmp/.android-emulator-XXXXXX"
    int   tempfd = -1;
    char  template[512];
    char  *p = template, *end = p + sizeof(template);

    p = bufprint_temp_file( p, end, "emulator-XXXXXX" );
    if (p >= end) {
        D( "Xcannot create temporary file in /tmp/android !!" );
        return NULL;
    }

    D( "template: %s", template );
    tempfd = mkstemp( template );
    if (tempfd < 0) {
        D("cannot create temporary file in /tmp/android !!");
        return NULL;
    }
    close(tempfd);
    tempname = template;
#endif
    tempfile = malloc( sizeof(*tempfile) + strlen(tempname) + 1 );
    tempfile->name = (char*)(tempfile + 1);
    strcpy( (char*)tempfile->name, tempname );

    tempfile->next = _all_tempfiles;
    _all_tempfiles = tempfile;

    if ( !tempfile->next ) {
        atexit( tempfile_atexit );
    }

    return tempfile;
}

const char*
tempfile_path(TempFile*  temp)
{
    return temp ? temp->name : NULL;
}

void
tempfile_close(TempFile*  tempfile)
{
#ifdef _WIN32
    DeleteFile(tempfile->name);
#else
    unlink(tempfile->name);
#endif
}

/** TEMP FILE CLEANUP
 **
 **/

/* we don't expect to use many temporary files */
#define MAX_ATEXIT_FDS  16

typedef struct {
    int   count;
    int   fds[ MAX_ATEXIT_FDS ];
} AtExitFds;

static void
atexit_fds_add( AtExitFds*  t, int  fd )
{
    if (t->count < MAX_ATEXIT_FDS)
        t->fds[t->count++] = fd;
    else {
        dwarning("%s: over %d calls. Program exit may not cleanup all temporary files",
            __FUNCTION__, MAX_ATEXIT_FDS);
    }
}

static void
atexit_fds_del( AtExitFds*  t, int  fd )
{
    int  nn;
    for (nn = 0; nn < t->count; nn++)
        if (t->fds[nn] == fd) {
            /* move the last element to the current position */
            t->count  -= 1;
            t->fds[nn] = t->fds[t->count];
            break;
        }
}

static void
atexit_fds_close_all( AtExitFds*  t )
{
    int  nn;
    for (nn = 0; nn < t->count; nn++)
        close(t->fds[nn]);
}

static AtExitFds   _atexit_fds[1];

void
atexit_close_fd(int  fd)
{
    if (fd >= 0)
        atexit_fds_add(_atexit_fds, fd);
}

void
atexit_close_fd_remove(int  fd)
{
    if (fd >= 0)
        atexit_fds_del(_atexit_fds, fd);
}

static void
tempfile_atexit( void )
{
    TempFile*  tempfile;

    atexit_fds_close_all( _atexit_fds );

    for (tempfile = _all_tempfiles; tempfile; tempfile = tempfile->next)
        tempfile_close(tempfile);
}
