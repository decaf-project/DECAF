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
#ifndef _ANDROID_GRAPHICS_REFLIST_H
#define _ANDROID_GRAPHICS_REFLIST_H

#include <inttypes.h>
#include <android/utils/system.h>

/* Definitions for a smart list of references to generic objects.
 * supports safe deletion and addition while they are being iterated
 * with AREFLIST_FOREACH() macro.
 *
 * note that you cannot add NULL to an ARefList.
 */

/* Clients should ignore these implementation details, which
 * we're going to explain there:
 *   - 'count' is the number of items in the list
 *   - 'size' is the number of slots in the list's array. It is
 *     always >= 'count'. Some slots correspond to deleted items
 *     and will hold a NULL value.
 *   - 'max' is the size of the slots array
 *   - 'u.item0' is used when 'max' is 1
 *   - 'u.items' is the slot array if 'max > 1'
 *   - 'u.next' is only used for free-list storage.
 */
typedef struct ARefList {
    /* XXX: should we use uint32_t instead ? */
    uint16_t   count, size, max;
    uint16_t   iteration;
    union {
        void*   item0;
        void**  items;
    } u;
} ARefList;

/* Initialize an empty ARefList */
AINLINED void
areflist_init(ARefList*  l)
{
    l->count     = 0;
    l->size      = 0;
    l->max       = 1;
    l->iteration = 0;
}

/* Return the number of items in a list */
AINLINED int
areflist_getCount(const ARefList*  l)
{
    return l->count;
}

/* Clear an ARefList */
void  areflist_setEmpty(ARefList*  l);

/* Finalize, i.e. clear, an ARefList */
AINLINED void
areflist_done(ARefList*  l)
{
    areflist_setEmpty(l);
}

/* Return TRUE iff an ARefList has no item */
AINLINED ABool
areflist_isEmpty(const ARefList*  l)
{
    return (areflist_getCount(l) == 0);
}

/* Return the index of 'item' in the ARefList, or -1.
 * This returns -1 if 'item' is NULL.
 */
int    areflist_indexOf(const ARefList*  l, void*  item);

/* Return TRUE iff an ARefList contains 'item' */
AINLINED ABool
areflist_has(const ARefList*  l, void*  item)
{
    return areflist_indexOf(l, item) >= 0;
}

/* Append 'item' to a list. An item can be added several
 * times to the same list. Do nothing if 'item' is NULL. */
void    areflist_add(ARefList*  l, void*  item);

/* Remove first instance of 'item' from an ARefList.
 * Returns TRUE iff the item was found in the list. */
ABool   areflist_delFirst(ARefList*  l, void*  item);

/* Remove all instances of 'item' from an ARefList.
 * returns TRUE iff the item was found in the list */
ABool   areflist_delAll(ARefList*  l, void*  item);

/* Same as areflist_add() */
AINLINED void
areflist_push(ARefList*  l, void*  item)
{
    areflist_add(l, item);
}

/* Remove last item from an ARefList and return it.
 * NULL is returned if the list is empty */
void*  areflist_popLast(ARefList*  l);

/* Return the n-th array entry, or NULL in case of invalid index */
void*   areflist_get(const ARefList*  l, int  n);

AINLINED int
areflist_count(ARefList*  l)
{
    return l->count;
}

void  areflist_append(ARefList*  l, const ARefList*  src);

/* used internally */
void    _areflist_remove_deferred(ARefList*  l);

void**  _areflist_at(const ARefList*  l, int  n);

#define  AREFLIST_LOOP(list_,itemvar_) \
    do { \
        ARefList*  _reflist_loop   = (list_); \
        int        _reflist_loop_i = 0; \
        int        _reflist_loop_n = _reflist_loop->size; \
        _reflist_loop->iteration += 2; \
        for ( ; _reflist_loop_i < _reflist_loop_n; _reflist_loop_i++ ) { \
            void** _reflist_loop_at = _areflist_at(_reflist_loop, _reflist_loop_i); \
            (itemvar_) = *(_reflist_loop_at); \
            if ((itemvar_) != NULL) {

#define  AREFLIST_LOOP_END \
            } \
        } \
        if (_reflist_loop->iteration & 1) \
            _areflist_remove_deferred(_reflist_loop); \
    } while (0);

#define  AREFLIST_LOOP_CONST(list_,itemvar_) \
    do { \
        const ARefList*  _reflist_loop   = (list_); \
        int              _reflist_loop_i = 0; \
        int              _reflist_loop_n = _reflist_loop->size; \
        for ( ; _reflist_loop_i < _reflist_loop_n; _reflist_loop_i++ ) { \
            void** _reflist_loop_at = _areflist_at(_reflist_loop, _reflist_loop_i); \
            (itemvar_) = *(_reflist_loop_at); \
            if ((itemvar_) != NULL) {

#define  AREFLIST_LOOP_DEL() \
    (_reflist_loop->iteration |= 1, *_reflist_loop_at = NULL)

#define  AREFLIST_LOOP_SET(val) \
    (_reflist_loop->iteration |= 1, *_reflist_loop_at = (val))


#define  AREFLIST_FOREACH(list_,item_,statement_) \
    ({ ARefList*  _reflist   = (list_); \
       int        _reflist_i = 0; \
       int        _reflist_n = _reflist->size; \
       _reflist->iteration += 2; \
       for ( ; _reflist_i < _reflist_n; _reflist_i++ ) { \
           void**  __reflist_at   = _areflist_at(_reflist, _reflist_i); \
           void*  item_ = *__reflist_at; \
           if (item_ != NULL) { \
               statement_; \
           } \
       } \
       _reflist->iteration -= 2; \
       if (_reflist->iteration == 1) \
           _areflist_remove_deferred(_reflist); \
    })

#define  AREFLIST_FOREACH_CONST(list_,item_,statement_) \
    ({ const ARefList*  _reflist = (list_); \
       int        _reflist_i = 0; \
       int        _reflist_n = _reflist->size; \
       for ( ; _reflist_i < _reflist_n; _reflist_i++ ) { \
           void**  __reflist_at = _areflist_at(_reflist, _reflist_i); \
           void*  item_ = *__reflist_at; \
           if (item_ != NULL) { \
               statement_; \
           } \
       } \
    })

/* use this to delete the currently iterated element */
#define  AREFLIST_DEL_ITERATED()  \
    ({ *__reflist_at = NULL; \
       _reflist->iteration |= 1; })

/* use this to replace the currently iterated element */
#define  AREFLIST_SET_ITERATED(item) \
    ({ *__reflist_at = (item); \
       if (item == NULL) _reflist->iteration |= 1; })

void  areflist_copy(ARefList*  dst, const ARefList*  src);

#endif /* _ANDROID_GRAPHICS_REFLIST_H */
