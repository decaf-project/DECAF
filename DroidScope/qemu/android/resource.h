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
#ifndef _ANDROID_RESOURCE_H
#define _ANDROID_RESOURCE_H

#include <stddef.h>

extern const unsigned char*
android_resource_find( const char*    name,
                       size_t        *psize );

extern const unsigned char*
android_icon_find( const char*   name,
                   size_t       *psize );

#endif /* END */

