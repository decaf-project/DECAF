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
#include "android/utils/reflist.h"
#include "android/utils/system.h"
#include "android/utils/assert.h"
#include <stdlib.h>
#include <string.h>

static void** _areflist_items(const ARefList*  l)
{
    if (l->max == 1)
        return (void**)&l->u.item0;
    else
        return l->u.items;
}

static void
_areflist_checkSize0(ARefList*  l)
{
    if (l->size == 0 && l->max > 1) {
        AFREE(l->u.items);
        l->max     = 1;
        l->u.item0 = NULL;
    }
}

void
areflist_setEmpty(ARefList*  l)
{
    if (l->iteration > 0) {
        /* deferred empty, set all items to NULL
         * to stop iterations */
        void**  items = _areflist_items(l);
        AARRAY_ZERO(items, l->count);
        l->iteration |= 1;
    } else {
        /* direct empty */
        l->size = 0;
        _areflist_checkSize0(l);
    }
    l->count = 0;
}

int
areflist_indexOf(const ARefList*  l, void*  item)
{
    if (item) {
        void**  items = _areflist_items(l);
        void**  end   = items + l->count;
        void**  ii    = items;

        for ( ; ii < end; ii += 1 )
            if (*ii == item)
                return (ii - items);
    }
    return -1;
}

static void
areflist_grow(ARefList*  l, int  count)
{
    int   newcount = l->size + count;
    if (newcount > l->max) {
        int  oldmax = l->max;
        int  newmax;
        void**  items;

        if (oldmax == 1) {
            oldmax   = 0;
            items    = NULL;
        } else {
            items = l->u.items;
        }
        newmax = oldmax;
        while (newmax < newcount)
            newmax += (newmax >> 1) + 4;

        AARRAY_RENEW(items, newmax);

        if (l->max == 1)
            items[0] = l->u.item0;

        l->u.items = items;
        l->max     = (uint16_t) newmax;
    }
}


void
areflist_add(ARefList*  l, void*  item)
{
    if (item) {
        void**  items;

        if (l->size >= l->max) {
            areflist_grow(l, 1);
        }
        items = _areflist_items(l);
        items[l->size] = item;
        l->size  += 1;
        l->count += 1;
    }
}

void
areflist_append(ARefList*  l, const ARefList*  l2)
{
    AREFLIST_FOREACH_CONST(l2, item, {
        areflist_add(l, item);
    });
}

void*
areflist_popLast(ARefList*  l)
{
    void*   item = NULL;
    void**  items = _areflist_items(l);
    int     nn;

    if (l->count == 0)
        return NULL;

    for (nn = l->size; nn > 0; nn--) {
        item = items[nn-1];
        if (item != NULL)
            goto FOUND_LAST;
    }
    return NULL;

FOUND_LAST:
    /* we found the last non-NULL item in the array */
    l->count   -= 1;
    items[nn-1] = NULL;

    if (l->iteration == 0) {
        l->size = nn;
        _areflist_checkSize0(l);
    }
    return item;
}

ABool
areflist_delFirst(ARefList*  l, void*  item)
{
    if (item == NULL)
        return 0;

    int  index = areflist_indexOf(l, item);
    if (index < 0)
        return 0;

    void** items = _areflist_items(l);

    if (l->iteration > 0) {
        /* deferred deletion */
        items[index]  = NULL;
        l->iteration |= 1;
        l->count     -= 1;
    } else {
        /* direct deletion */
        if (l->max > 1) {
            AARRAY_MOVE(items + index, items + index + 1, l->size - index - 1);
            l->size -= 1;
    l->count -= 1;
            _areflist_checkSize0(l);
        } else {
            l->u.item0 = NULL;
            l->size    = 0;
            l->count   = 0;
        }
    }
    return 1;
}

ABool
areflist_delAll(ARefList*  l, void*  item)
{
    ABool   result = 0;

    if (item == NULL)
        return 0;

    void**  items    = _areflist_items(l);
    int     readPos  = 0;
    int     writePos = 0;

    /* don't modify the list until we find the item
     * or an EMPTY slot */
    for ( ; readPos < l->size; readPos++ ) {
        if (items[readPos] == NULL || items[readPos] == item)
            goto COPY_LIST;
    }
    return 0;

COPY_LIST:
    writePos = readPos;
    for ( ; readPos < l->size; readPos++ ) {
        if (items[readPos] == NULL) {
            continue;
        }
        if (items[readPos] == item) {
            result = 1;
            continue;
        }
        items[writePos] = items[readPos];
        writePos++;
    }
    l->count = l->size = (uint16_t) writePos;
    _areflist_checkSize0(l);

    return result;
}


void
_areflist_remove_deferred(ARefList*  l)
{
    if (l->iteration & 1) {
        /* remove all NULL elements from the array */
        void**  items = _areflist_items(l);
        int     rr = 0;
        int     ww = 0;
        for ( ; rr < l->size; rr++ ) {
            if (items[rr] != NULL)
                items[ww++] = items[rr];
        }
        l->count = l->size = (uint16_t) ww;
        _areflist_checkSize0(l);
    }
    l->iteration = 0;
}

void
areflist_copy(ARefList*  dst, const ARefList*  src)
{
    dst[0] = src[0];

    if (src->max > 1) {
        dst->max  = dst->count;
        AARRAY_NEW(dst->u.items, dst->max);

        void**  ritems = src->u.items;
        void**  witems = _areflist_items(dst);
        int  rr = 0;
        int  ww = 0;
        for ( ; rr < src->size; rr++ ) {
            if (ritems[rr] != NULL) {
                witems[ww++] = ritems[rr];
            }
        }
        dst->size = ww;
        AASSERT_TRUE(ww == dst->count);
        _areflist_checkSize0(dst);
    }
}

void*
areflist_get(const ARefList*  l, int  n)
{
    if ((unsigned)n >= (unsigned)l->count)
        return NULL;

    if (l->max == 1)
        return l->u.item0;

    return l->u.items[n];
}

void**
_areflist_at(const ARefList*  l, int  n)
{
    void**  items;

    if ((unsigned)n >= (unsigned)l->size)
        return NULL;

    items = _areflist_items(l);
    return items + n;
}
