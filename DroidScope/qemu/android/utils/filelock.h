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

#ifndef _ANDROID_UTILS_FILELOCK_H
#define _ANDROID_UTILS_FILELOCK_H

/** FILE LOCKS SUPPORT
 **
 ** a FileLock is useful to prevent several emulator instances from using the same
 ** writable file (e.g. the userdata.img disk images).
 **
 ** create a FileLock object with filelock_create(), the function will return
 ** NULL only if the corresponding path is already locked by another emulator
 ** of if the path is read-only.
 **
 ** note that 'path' can designate a non-existing path and that the lock creation
 ** function can detect stale file locks that can longer when the emulator
 ** crashes unexpectedly, and will happily clean them for you.
 **
 ** you can call filelock_release() to release a file lock explicitely. otherwise
 ** all file locks are automatically released when the program exits.
 **/

typedef struct FileLock  FileLock;

extern FileLock*  filelock_create ( const char*  path );
extern void       filelock_release( FileLock*  lock );

#endif /* _ANDROID_UTILS_FILELOCK_H */
