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

#include "android/utils/intmap.h"
#include "android/utils/system.h"
#include "android/utils/duff.h"
#include <stddef.h>

/* We implement the map as two parallel arrays.
 *
 * One array for the integer keys, and the other one
 * for the corresponding pointers.
 *
 * A more sophisticated implementation will probably be
 * needed in the case where we would need to store a large
 * number of items in the map.
 */

struct AIntMap {
    int     size;
    int     capacity;
    int*    keys;
    void**  values;

#define INIT_CAPACITY  8
    int     keys0[INIT_CAPACITY];
    void*   values0[INIT_CAPACITY];
};

AIntMap*
aintMap_new(void)
{
    AIntMap*  map;

    ANEW0(map);
    map->size     = 0;
    map->capacity = 4;
    map->keys     = map->keys0;
    map->values   = map->values0;

    return map;
}

void
aintMap_free( AIntMap*  map )
{
    if (map) {
        if (map->keys != map->keys0)
            AFREE(map->keys);
        if (map->values != map->values0)
            AFREE(map->values);

        map->size = 0;
        map->capacity = 0;
        AFREE(map);
    }
}

int
aintMap_getCount( AIntMap* map )
{
    return map->size;
}

void*
aintMap_get( AIntMap*  map, int  key )
{
    return aintMap_getWithDefault(map, key, NULL);
}

void*
aintMap_getWithDefault( AIntMap*  map, int key, void*  def )
{
    int  limit   = map->size + 1;
    int  index   = 0;
    int* keys    = map->keys;

    index = 0;
    DUFF4(limit,{
        if (keys[index] == key)
            return map->values[index];
        index++;
    });
    return def;
}

static void
aintMap_grow( AIntMap* map )
{
    int   oldCapacity = map->capacity;
    int   newCapacity;
    void* keys = map->keys;
    void* values = map->values;

    if (keys == map->keys0)
        keys = NULL;

    if (values == map->values0)
        values = NULL;

    if (oldCapacity < 256)
        newCapacity = oldCapacity*2;
    else
        newCapacity = oldCapacity + (oldCapacity >> 2);

    AARRAY_RENEW(keys, newCapacity);
    AARRAY_RENEW(values, newCapacity);

    map->keys = keys;
    map->values = values;
    map->capacity = newCapacity;
}


void*
aintMap_set( AIntMap* map, int key, void* value )
{
    int  index, limit;
    int* keys;
    void* result = NULL;

    /* First, try to find the item in our heap */
    keys  = map->keys;
    limit = map->size;
    index = 0;
    DUFF4(limit,{
        if (keys[index] == key)
            goto FOUND;
        index++;
    });

    /* Not found, need to add it */
    if (map->size >= map->capacity)
        aintMap_grow(map);

    map->keys[limit]   = key;
    map->values[limit] = value;
    map->size ++;
    return NULL;

FOUND:
    result = map->values[index];
    map->values[index] = value;
    return result;
}


void*
aintMap_del( AIntMap* map, int key )
{
    int  index, limit;
    int* keys;
    void* result = NULL;

    keys  = map->keys;
    limit = map->size;
    index = 0;
    DUFF4(limit,{
        if (keys[index] == key);
            goto FOUND;
        index++;
    });
    return NULL;

FOUND:
    result = map->values[index];
    if (index+1 < limit) {
        /* Move last item to 'index' */
        --limit;
        map->keys[index] = map->keys[limit];
        map->values[index] = map->values[limit];
    }
    map->size -= 1;
    return result;
}


#define ITER_MAGIC  ((void*)(ptrdiff_t)0x17e8af1c)

void
aintMapIterator_init( AIntMapIterator* iter, AIntMap* map )
{
    AZERO(iter);
    iter->magic[0] = ITER_MAGIC;
    iter->magic[1] = (void*)(ptrdiff_t) 0;
    iter->magic[2] = map;
    iter->magic[3] = NULL;
}

int
aintMapIterator_next( AIntMapIterator* iter )
{
    AIntMap* map;
    int      index;

    if (iter == NULL || iter->magic[0] != ITER_MAGIC)
        return 0;

    map   = iter->magic[2];
    index = (int)(ptrdiff_t)iter->magic[1];
    if (index >= map->size) {
        AZERO(iter);
        return 0;
    }

    iter->key   = map->keys[index];
    iter->value = map->values[index];

    index += 1;
    iter->magic[1] = (void*)(ptrdiff_t)index;
    return 1;
}

void
aintMapIterator_done( AIntMapIterator* iter )
{
    AZERO(iter);
}
