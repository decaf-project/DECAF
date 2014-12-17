/* Copyright (C) 2009 The Android Open Source Project
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
#ifndef _ANDROID_UTILS_VECTOR_H
#define _ANDROID_UTILS_VECTOR_H

#include "android/utils/system.h"
#include "android/utils/assert.h"

#define  AVECTOR_DECL(ctype,name)  \
    ctype*    name; \
    unsigned  num_##name; \
    unsigned  max_##name \

#define  AVECTOR_SIZE(obj,name)  \
    (obj)->num_##name

#define  AVECTOR_INIT(obj,name)   \
    do { \
        (obj)->name = NULL; \
        (obj)->num_##name = 0; \
        (obj)->max_##name = 0; \
    } while (0)

#define  AVECTOR_INIT_ALLOC(obj,name,count) \
    do { \
        AARRAY_NEW0( (obj)->name, (count) ); \
        (obj)->num_##name = 0; \
        (obj)->max_##name = (count); \
    } while (0)

#define  AVECTOR_DONE(obj,name)  \
    do { \
        AFREE((obj)->name); \
        (obj)->num_##name = 0; \
        (obj)->max_##name = 0; \
    } while (0)

#define  AVECTOR_CLEAR(obj,name) \
    do { \
        (obj)->num_##name = 0; \
    } while (0)

#define  AVECTOR_AT(obj,name,index)  \
    (&(obj)->name[(index)])

#define  AVECTOR_REALLOC(obj,name,newMax) \
    do { \
        AARRAY_RENEW((obj)->name,newMax); \
        (obj)->max_##name = (newMax); \
    } while(0)

#define  AVECTOR_ENSURE(obj,name,newCount) \
    do { \
        unsigned  _newCount = (newCount); \
        if (_newCount > (obj)->max_##name) \
            AASSERT_LOC(); \
            _avector_ensure( (void**)&(obj)->name, sizeof((obj)->name[0]), \
                             &(obj)->max_##name, _newCount ); \
    } while (0);

extern void  _avector_ensure( void**  items, size_t  itemSize,
                              unsigned*  pMaxItems, unsigned  newCount );

#define  AVECTOR_FOREACH(obj,name,itemptr,statement) \
    do { \
        unsigned __vector_nn = 0; \
        unsigned __vector_max = (obj)->num_##name; \
        for ( ; __vector_nn < __vector_max; __vector_nn++ ) { \
            itemptr = &(obj)->name[__vector_nn]; \
            statement; \
        } \
    } while (0);

/* */

#endif /* _ANDROID_UTILS_VECTOR_H */
