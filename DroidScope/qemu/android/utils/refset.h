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
#ifndef _ANDROID_UTILS_REFSET_H
#define _ANDROID_UTILS_REFSET_H

#include <android/utils/vector.h>

/* this implements a set of addresses in memory.
 * NULL cannot be stored in the set.
 */

typedef struct {
    AVECTOR_DECL(void*,buckets);
    int iteration;
} ARefSet;

AINLINED void
arefSet_init( ARefSet*  s )
{
    AVECTOR_INIT(s,buckets);
}

AINLINED void
arefSet_done( ARefSet*  s )
{
    AVECTOR_DONE(s,buckets);
}

AINLINED void
arefSet_clear( ARefSet*  s )
{
    AVECTOR_CLEAR(s,buckets);
    s->iteration = 0;
}

AINLINED int
arefSet_count( ARefSet*  s )
{
    return (int) AVECTOR_SIZE(s,buckets);
}

extern ABool  arefSet_has( ARefSet*  s, void*  item );
extern void   arefSet_add( ARefSet*  s, void*  item );
extern void   arefSet_del( ARefSet*  s, void*  item );

extern void   _arefSet_removeDeferred( ARefSet* s );

#define  AREFSET_DELETED ((void*)~(size_t)0)

#define  AREFSET_FOREACH(_set,_item,_statement) \
    do { \
        int  __refset_nn = 0; \
        int  __refset_max = (_set)->max_buckets; \
        (_set)->iteration += 2; \
        for ( ; __refset_nn < __refset_max; __refset_nn++ ) { \
            void*  __refset_item = (_set)->buckets[__refset_nn]; \
            if (__refset_item == NULL || __refset_item == AREFSET_DELETED) \
                continue; \
            _item = __refset_item; \
            _statement; \
        } \
        (_set)->iteration -= 2; \
        if ((_set)->iteration == 1) \
            _arefSet_removeDeferred(_set); \
    } while (0)

#endif /* _ANDROID_UTILS_REFSET_H */
