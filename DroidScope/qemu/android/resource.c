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
#include "android/resource.h"
#include "config-host.h"
#include <string.h>

typedef struct {
    const char*           name;
    const unsigned char*  base;
    size_t                size;
} FileEntry;

const unsigned char*
_resource_find( const char*       name,
                const FileEntry*  entries,
                size_t           *psize )
{
    const FileEntry*  e = entries;

    for ( ; e->name != NULL; e++ ) {
        //dprint("SCAN %s\n", e->name);
        if ( strcmp(e->name, name) == 0 ) {
            *psize = e->size;
            return e->base;
        }
    }
    return NULL;
}

#undef   _file_entries
#define  _file_entries  _skin_entries
const unsigned char*
android_resource_find( const char*  name,
                       size_t      *psize )
{
#    include "android/skin/default.h"
    return _resource_find( name, _file_entries, psize );
}

#undef   _file_entries
#define  _file_entries  _icon_entries

const unsigned char*
android_icon_find( const char*  name,
                   size_t      *psize )
{
#ifdef _WIN32
    return NULL;
#else
#   include "android/icons.h"
    return _resource_find( name, _file_entries, psize );
#endif
}


