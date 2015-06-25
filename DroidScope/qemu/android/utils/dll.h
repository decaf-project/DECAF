/* Copyright (C) 2011 The Android Open Source Project
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
#ifndef ANDROID_UTILS_DLL_H
#define ANDROID_UTILS_DLL_H

/* Opaque type to model a dynamic library handle */
typedef struct ADynamicLibrary   ADynamicLibrary;

/* Try to load/open a dynamic library named 'libraryName', looking for
 * it in the optional paths listed by 'libraryPaths'.
 *
 * Once opened, you can use adynamicLibrary_findSymbol() and
 * adynamicLibrary_close() on it.
 *
 * libraryName :: library name, if no extension is provided, then '.so'
 *                will be appended on Unix systems, or '.dll' on Windows.
 *
 * pError :: On success, '*pError' will be set to NULL. On error, it will
 *           point to a string describing the error, which must be freed by
 *           the caller.
 *
 * returns an ADynamicLibrary pointer.
 */
ADynamicLibrary*   adynamicLibrary_open( const char*  libraryName,
                                         char**       pError);

/* Find a symbol inside a dynamic library. */
void* adynamicLibrary_findSymbol( ADynamicLibrary*  lib,
                                  const char*       symbolName,
                                  char**            pError);

/* Close/unload a given dynamic library */
void  adynamicLibrary_close( ADynamicLibrary*  lib );

#endif /* ANDROID_UTILS_DLL_H */
