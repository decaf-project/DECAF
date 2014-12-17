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
#ifndef _ANDROID_UTILS_DIR_H
#define _ANDROID_UTILS_DIR_H

/* simple utility to parse directories for files            */
/* needed because Unix and Windows don't use the same stuff */

typedef struct DirScanner  DirScanner;

/* Create a new directory scanner object from a given rootPath.
 * returns NULL in case of failure (error code in errno)
 */
DirScanner*    dirScanner_new ( const char*  rootPath );

/* Destroy a given directory scanner. You must always call
 * this function to release proper system resources.
 */
void           dirScanner_free( DirScanner*  s );

/* Get the name of the next file from a directory scanner.
 * Returns NULL when there is no more elements in the list.
 *
 * This is only the file name, use dirScanner_nextFull to
 * get its full path.
 *
 * This will never return '.' and '..'.
 *
 * The returned string is owned by the scanner, and will
 * change on the next call to this function or when the
 * scanner is destroyed.
 */
const char*    dirScanner_next( DirScanner*  s );

/* A variant of dirScanner_next() which returns the full path
 * to the next directory element.
 */
const char*    dirScanner_nextFull( DirScanner*  s );

/* */

#endif /* _ANDROID_UTILS_DIR_H */
