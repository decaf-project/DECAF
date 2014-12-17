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
 * Contains declarations of structures and routines that implement a red-black
 * tree (a map) of module memory mapping ranges in the guest system. The map is
 * organized in such a way, that each entry in the map describes a virtual
 * address range that belongs to a mapped execution module in the guest system.
 * Map considers two ranges to be equal if their address ranges intersect in
 * any part.
 */

#ifndef QEMU_MEMCHECK_MEMCHECK_MMRANGE_MAP_H
#define QEMU_MEMCHECK_MEMCHECK_MMRANGE_MAP_H

#include "sys-tree.h"
#include "memcheck_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Memory mapping range map. */
typedef struct MMRangeMap {
    /* Head of the map. */
    struct MMRangeMapEntry* rbh_root;
} MMRangeMap;

// =============================================================================
// Map API
// =============================================================================

/* Initializes the map.
 * Param:
 *  map - Map to initialize.
 */
void mmrangemap_init(MMRangeMap* map);

/* Inserts new (or replaces existing) entry in the map.
 * Insertion, or replacement is controlled by the value, passed to this routine
 * with 'replaced' parameter. If this parameter is NULL, insertion will fail if
 * a matching entry already exists in the map. If 'replaced' parameter is not
 * NULL, and a matching entry exists in the map, content of the existing entry
 * will be copied to the descriptor, addressed by 'replace' parameter, existing
 * entry will be removed from the map, and new entry will be inserted.
 * Param:
 *  map - Map where to insert new, or replace existing entry.
 *  desc - Descriptor to insert to the map.
 *  replaced - If not NULL, upon return from this routine contains descriptor
 *      that has been replaced in the map with the new entry. Note that if this
 *      routine returns with value other than RBT_MAP_RESULT_ENTRY_REPLACED,
 *      content of the 'replaced' buffer is not defined, as no replacement has
 *      actually occurred.
 * Return
 *  See RBTMapResult for the return codes.
 */
RBTMapResult mmrangemap_insert(MMRangeMap* map,
                               const MMRangeDesc* desc,
                               MMRangeDesc* replaced);

/* Finds an entry in the map that matches the given address.
 * Param:
 *  map - Map where to search for an entry.
 *  start - Starting address of a mapping range.
 *  end - Ending address of a mapping range.
 * Return:
 *  Pointer to the descriptor found in a map entry, or NULL if no matching
 * entry has been found in the map.
 */
MMRangeDesc* mmrangemap_find(const MMRangeMap* map,
                             target_ulong start,
                             target_ulong end);

/* Pulls (finds and removes) an entry from the map that matches the given
 * address.
 * Param:
 *  map - Map where to search for an entry.
 *  start - Starting address of a mapping range.
 *  end - Ending address of a mapping range.
 *  pulled - Upon successful return contains descriptor data pulled from the
 *      map.
 * Return:
 *  Zero if a descriptor that matches the given address has been pulled, or 1
 *  if no matching entry has been found in the map.
 */
int mmrangemap_pull(MMRangeMap* map,
                    target_ulong start,
                    target_ulong end,
                    MMRangeDesc* pulled);

/* Copies content of one memory map to another.
 * Param:
 *  to - Map where to copy entries to.
 *  from - Map where to copy entries from.
 * Return:
 *  Zero on success, or -1 on error.
 */
int mmrangemap_copy(MMRangeMap* to, const MMRangeMap* from);

/* Empties the map.
 * Param:
 *  map - Map to empty.
 * Return:
 *  Number of entries removed from the map.
 */
int mmrangemap_empty(MMRangeMap* map);

#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif  // QEMU_MEMCHECK_MEMCHECK_MMRANGE_MAP_H
