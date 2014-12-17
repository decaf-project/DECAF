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
#ifndef _ANDROID_UTILS_INTMAP_H
#define _ANDROID_UTILS_INTMAP_H

/* A simple container that can hold a simple mapping from integers to
 * references. I.e. a dictionary where keys are integers, and values
 * are liberal pointer values (NULL is allowed).
 */

typedef struct AIntMap  AIntMap;

/* Create new integer map */
AIntMap*  aintMap_new(void);

/* Returns the number of keys stored in the map */
int       aintmap_getCount( AIntMap* map );

/* Returns TRUE if the map has a value for the 'key'. Necessary because
 * NULL is a valid value for the map.
 */
int       aintmap_has( AIntMap*  map, int key );

/* Get the value associated with a 'key', or NULL if not in map */
void*     aintMap_get( AIntMap*  map, int  key );

/* Get the value associated with a 'key', or 'def' if not in map */
void*     aintMap_getWithDefault( AIntMap*  map, int key, void*  def );

/* Set the value associated to a 'key', return the old value, if any, or NULL */
void*     aintMap_set( AIntMap* map, int key, void* value );

/* Delete a given value associated to a 'key', return the old value, or NULL */
void*     aintMap_del( AIntMap* map, int key );

/* Destroy a given integer map */
void      aintMap_free( AIntMap*  map );

/* Integer map iterator. First call aintMapIterator_init(), then call
 * aintMapIterator_next() until it returns 0. Then finish with
 * aintMapIterator_done().
 *
 * Example:
 *    AIntMapIterator  iter[1];
 *    aintMapIterator_init(iter, map);
 *    while (aintMapIterator_next(iter, &key, &value)) {
 *        // do something
 *    }
 *    aintMapIterator_done(iter);
 */
typedef struct AIntMapIterator {
    int    key;
    void*  value;
    void*  magic[4];
} AIntMapIterator;

/* Initialize iterator. Returns -1 if the map is empty, or 0 otherwise
 * On success, the first (key,value) pair can be read from the iterator
 * directly.
 */
void aintMapIterator_init( AIntMapIterator* iter, AIntMap* map );

/* Read the next (key,value) pair with an iterator, returns -1 when
 * there isn't anything more, or 0 otherwise. On success, the key and
 * value can be read directly from the iterator.
 */
int  aintMapIterator_next( AIntMapIterator* iter );

/* Finalize an iterator. This only needs to be called if you stop
 * the iteration before aintMapIterator_init() or aintMapIterator_next()
 * return -1.
 */
void aintMapIterator_done( AIntMapIterator* iter );

#define AINTMAP_FOREACH_KEY(map, keyvarname, stmnt) \
    do { \
        AIntMapIterator  __aintmap_foreach_iter[1]; \
        aintMapIterator_init(__aintmap_foreach_iter, (map)); \
        while (aintMapIterator_next(__aintmap_foreach_iter)) { \
            int keyvarname = __aintmap_foreach_iter->key; \
            stmnt; \
        } \
        aintMapIterator_done(__aintmap_foreach_iter); \
    } while (0)

#define AINTMAP_FOREACH_VALUE(map, valvarname, stmnt) \
    do { \
        AIntMapIterator  __aintmap_foreach_iter[1]; \
        aintMapIterator_init(__aintmap_foreach_iter, (map)); \
        while (aintMapIterator_next(__aintmap_foreach_iter)) { \
            void* valvarname = __aintmap_foreach_iter->value; \
            stmnt; \
        } \
        aintMapIterator_done(__aintmap_foreach_iter); \
    } while (0)

#endif /* _ANDROID_UTILS_INTMAP_H */
