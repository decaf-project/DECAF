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

#ifndef _ANDROID_UTILS_TEMPFILE_H
#define _ANDROID_UTILS_TEMPFILE_H

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

typedef struct TempFile   TempFile;

extern  TempFile*    tempfile_create( void );
extern  const char*  tempfile_path( TempFile*  temp );
extern  void         tempfile_close( TempFile*  temp );

/** TEMP FILE CLEANUP
 **
 ** We delete all temporary files in atexit()-registered callbacks.
 ** however, the Win32 DeleteFile is unable to remove a file unless
 ** all HANDLEs to it are closed in the terminating process.
 **
 ** Call 'atexit_close_fd' on a newly open-ed file descriptor to indicate
 ** that you want it closed in atexit() time. You should always call
 ** this function unless you're certain that the corresponding file
 ** cannot be temporary.
 **
 ** Call 'atexit_close_fd_remove' before explicitely closing a 'fd'
 **/
extern void          atexit_close_fd(int  fd);
extern void          atexit_close_fd_remove(int  fd);

#endif /* _ANDROID_UTILS_TEMPFILE_H */
