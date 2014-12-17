/* Copyright (C) 2007-2010 The Android Open Source Project
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

/*
 * Contains implementation of routines that implement a red-black tree of
 * memory mappings in the guest system.
 */

#include "memcheck_mmrange_map.h"
#include "memcheck_logging.h"

/* Memory range descriptor stored in the map. */
typedef struct MMRangeMapEntry {
    /* R-B tree entry. */
    RB_ENTRY(MMRangeMapEntry)   rb_entry;

    /* Memory range descriptor for this entry. */
    MMRangeDesc                 desc;
} MMRangeMapEntry;

// =============================================================================
// R-B Tree implementation
// =============================================================================

/* Compare routine for the map.
 * Param:
 *  d1 - First map entry to compare.
 *  d2 - Second map entry to compare.
 * Return:
 *  0 - Descriptors are equal. Note that descriptors are considered to be
 *      equal iff memory blocks they describe intersect in any part.
 *  1 - d1 is greater than d2
 *  -1 - d1 is less than d2.
 */
static inline int
cmp_rb(MMRangeMapEntry* d1, MMRangeMapEntry* d2)
{
    const target_ulong start1 = d1->desc.map_start;
    const target_ulong start2 = d2->desc.map_start;

    if (start1 < start2) {
        return (d1->desc.map_end - 1) < start2 ? -1 : 0;
    }
    return (d2->desc.map_end - 1) < start1 ? 1 : 0;
}

/* Expands RB macros here. */
RB_GENERATE(MMRangeMap, MMRangeMapEntry, rb_entry, cmp_rb);

// =============================================================================
// Static routines
// =============================================================================

/* Inserts new (or replaces existing) entry into the map.
 * See comments on mmrangemap_insert routine in the header file for details
 * about this routine.
 */
static RBTMapResult
mmrangemap_insert_desc(MMRangeMap* map,
                       MMRangeMapEntry* rdesc,
                       MMRangeDesc* replaced)
{
    MMRangeMapEntry* existing = MMRangeMap_RB_INSERT(map, rdesc);
    if (existing == NULL) {
        return RBT_MAP_RESULT_ENTRY_INSERTED;
    }

    // Matching entry exists. Lets see if we need to replace it.
    if (replaced == NULL) {
        return RBT_MAP_RESULT_ENTRY_ALREADY_EXISTS;
    }

    /* Copy existing entry to the provided buffer and replace it
     * with the new one. */
    memcpy(replaced, &existing->desc, sizeof(MMRangeDesc));
    MMRangeMap_RB_REMOVE(map, existing);
    qemu_free(existing);
    MMRangeMap_RB_INSERT(map, rdesc);
    return RBT_MAP_RESULT_ENTRY_REPLACED;
}

/* Finds an entry in the map that matches the given address range.
 * Param:
 *  map - Map where to search for an entry.
 *  start - Starting address of a mapping range.
 *  end - Ending address of a mapping range.
 * Return:
 *  Address of a map entry that matches the given range, or NULL if no
 *  such entry has been found.
 */
static inline MMRangeMapEntry*
mmrangemap_find_entry(const MMRangeMap* map,
                      target_ulong start,
                      target_ulong end)
{
    MMRangeMapEntry rdesc;
    rdesc.desc.map_start = start;
    rdesc.desc.map_end = end;
    return MMRangeMap_RB_FIND((MMRangeMap*)map, &rdesc);
}

// =============================================================================
// Map API
// =============================================================================

void
mmrangemap_init(MMRangeMap* map)
{
    RB_INIT(map);
}

RBTMapResult
mmrangemap_insert(MMRangeMap* map,
                  const MMRangeDesc* desc,
                  MMRangeDesc* replaced)
{
    RBTMapResult ret;

    // Allocate and initialize new map entry.
    MMRangeMapEntry* rdesc = qemu_malloc(sizeof(MMRangeMapEntry));
    if (rdesc == NULL) {
        ME("memcheck: Unable to allocate new MMRangeMapEntry on insert.");
        return RBT_MAP_RESULT_ERROR;
    }
    memcpy(&rdesc->desc, desc, sizeof(MMRangeDesc));

    // Insert new entry into the map.
    ret = mmrangemap_insert_desc(map, rdesc, replaced);
    if (ret == RBT_MAP_RESULT_ENTRY_ALREADY_EXISTS ||
        ret == RBT_MAP_RESULT_ERROR) {
        /* Another descriptor already exists for this block, or an error
         * occurred. We have to free new descriptor, as it wasn't inserted. */
        qemu_free(rdesc);
    }
    return ret;
}

MMRangeDesc*
mmrangemap_find(const MMRangeMap* map, target_ulong start, target_ulong end)
{
    MMRangeMapEntry* rdesc = mmrangemap_find_entry(map, start, end);
    return rdesc != NULL ? &rdesc->desc : NULL;
}

int
mmrangemap_pull(MMRangeMap* map,
                target_ulong start,
                target_ulong end,
                MMRangeDesc* pulled)
{
    MMRangeMapEntry* rdesc = mmrangemap_find_entry(map, start, end);
    if (rdesc != NULL) {
        memcpy(pulled, &rdesc->desc, sizeof(MMRangeDesc));
        MMRangeMap_RB_REMOVE(map, rdesc);
        qemu_free(rdesc);
        return 0;
    } else {
        return -1;
    }
}

int
mmrangemap_pull_first(MMRangeMap* map, MMRangeDesc* pulled)
{
    MMRangeMapEntry* first = RB_MIN(MMRangeMap, map);
    if (first != NULL) {
        memcpy(pulled, &first->desc, sizeof(MMRangeDesc));
        MMRangeMap_RB_REMOVE(map, first);
        qemu_free(first);
        return 0;
    } else {
        return -1;
    }
}

int
mmrangemap_copy(MMRangeMap* to, const MMRangeMap* from)
{
    MMRangeMapEntry* entry;
    RB_FOREACH(entry, MMRangeMap, (MMRangeMap*)from) {
        RBTMapResult ins_res;
        MMRangeMapEntry* new_entry =
                (MMRangeMapEntry*)qemu_malloc(sizeof(MMRangeMapEntry));
        if (new_entry == NULL) {
            ME("memcheck: Unable to allocate new MMRangeMapEntry on copy.");
            return -1;
        }
        memcpy(new_entry, entry, sizeof(MMRangeMapEntry));
        new_entry->desc.path = qemu_malloc(strlen(entry->desc.path) + 1);
        if (new_entry->desc.path == NULL) {
            ME("memcheck: Unable to allocate new path for MMRangeMapEntry on copy.");
            qemu_free(new_entry);
            return -1;
        }
        strcpy(new_entry->desc.path, entry->desc.path);
        ins_res = mmrangemap_insert_desc(to, new_entry, NULL);
        if (ins_res != RBT_MAP_RESULT_ENTRY_INSERTED) {
            ME("memcheck: Unable to insert new range map entry on copy. Insert returned %u",
               ins_res);
            qemu_free(new_entry->desc.path);
            qemu_free(new_entry);
            return -1;
        }
    }

    return 0;
}

int
mmrangemap_empty(MMRangeMap* map)
{
    MMRangeDesc pulled;
    int removed = 0;

    while (!mmrangemap_pull_first(map, &pulled)) {
        qemu_free(pulled.path);
        removed++;
    }

    return removed;
}
