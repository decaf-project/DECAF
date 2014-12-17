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
#include <android/utils/refset.h>
#include <stddef.h>

#define  AREFSET_STEP    5

AINLINED uint32_t
_arefSet_hashItem( void*  item )
{
    uint32_t  hash;

    hash = (uint32_t)(ptrdiff_t)item >> 2;
    if (sizeof(item) > 4)
        hash ^= ((uint64_t)(ptrdiff_t)item >> 32);

    return hash;
}

static void**
_arefSet_lookup( ARefSet*  s, void*  item)
{
    uint32_t  hash = _arefSet_hashItem(item);
    unsigned  index = hash & (s->max_buckets-1);

    for (;;) {
        void**  pnode = &s->buckets[index];

        if (*pnode == item || *pnode == NULL)
            return pnode;

        index += AREFSET_STEP;
        if (index >= s->max_buckets)
            index -= s->max_buckets;
    }
}

static void**
_arefSet_lookupInsert( ARefSet*  s, void*  item)
{
    uint32_t  hash = _arefSet_hashItem(item);
    unsigned  index = hash & (s->max_buckets-1);
    void**    insert = NULL;

    for (;;) {
        void**  pnode = &s->buckets[index];

        if (*pnode == NULL) {
            return insert ? insert : pnode;
        } else if (*pnode == AREFSET_DELETED) {
            if (!insert)
                insert = pnode;
        } else if (*pnode == item)
            return pnode;

        index = (index + AREFSET_STEP) & (s->max_buckets-1);
    }
}

extern ABool
arefSet_has( ARefSet*  s, void*  item )
{
    void**  lookup;

    if (item == NULL || s->max_buckets == 0)
        return 0;

    lookup = _arefSet_lookup(s, item);
    return (*lookup == item);
}

/* the code below assumes, in the case of a down-size,
 * that there aren't too many items in the set.
 */
static void
_arefSet_resize( ARefSet*  s, unsigned  newSize )
{
    ARefSet   newSet;
    unsigned  nn, count = s->num_buckets;

    AVECTOR_INIT_ALLOC(&newSet,buckets, newSize);

    for (nn = 0; nn < s->max_buckets; nn++) {
        void*  item  = s->buckets[nn];
        if (item != NULL && item != AREFSET_DELETED) {
            void** lookup = _arefSet_lookup(&newSet, item);
            *lookup = item;
        }
    }

    AVECTOR_DONE(s,buckets);
    s->buckets     = newSet.buckets;
    s->num_buckets = count;
    s->max_buckets = newSet.max_buckets;
}

extern void
arefSet_add( ARefSet*  s, void*  item )
{
    void**  lookup;

    if (item == NULL)
        return;

    /* You can't add items to a set during iteration! */
    AASSERT(s->iteration == 0);

    if (s->max_buckets == 0)
        AVECTOR_INIT_ALLOC(s,buckets,4);

    lookup = _arefSet_lookupInsert(s, item);
    if (*lookup == item)
        return;

    *lookup = item;
    s->num_buckets += 1;

    if (s->iteration == 0) {
        if (s->num_buckets > s->max_buckets*0.85)
            _arefSet_resize(s, s->max_buckets*2);
    }
}

extern void
arefSet_del( ARefSet*  s, void*  item )
{
    void**  lookup;

    if (item == NULL || s->max_buckets == 0)
        return;

    lookup = _arefSet_lookup(s, item);
    if (*lookup != item)
        return;

    if (s->iteration == 0) {
        /* direct deletion */
        *lookup = NULL;
        s->num_buckets -= 1;
        if (s->num_buckets < s->max_buckets*0.25)
            _arefSet_resize(s, s->max_buckets/2);
    } else {
        /* deferred deletion */
        *lookup = AREFSET_DELETED;
        s->num_buckets -= 1;
        s->iteration   |= 1;
    }
}

void
_arefSet_removeDeferred( ARefSet*  s )
{
    unsigned nn, newSize;

    for (nn = 0; nn < s->max_buckets; nn++) {
        if (s->buckets[nn] == AREFSET_DELETED) {
            s->buckets[nn]  = NULL;
        }
    }
    s->iteration = 0;

    newSize = s->max_buckets;
    while (s->num_buckets < newSize*0.25)
        newSize /= 2;

    if (newSize != s->max_buckets)
        _arefSet_resize(s, newSize);
}

