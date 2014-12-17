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
 * tree (a map) of memory blocks allocated by the guest system. The map is
 * organized in such a way, that each entry in the map describes a virtual
 * address range that belongs to a memory block allocated in the guest's space.
 * The range includes block's suffix and prefix, as well as block returned to
 * malloc's caller. Map considers two blocks to be equal if their address ranges
 * intersect in any part. Allocation descriptor maps are instantiated one per
 * each process running on the guest system.
 */

#ifndef QEMU_MEMCHECK_MEMCHECK_MALLOC_MAP_H
#define QEMU_MEMCHECK_MEMCHECK_MALLOC_MAP_H

#include "sys-tree.h"
#include "memcheck_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Allocation descriptors map. */
typedef struct AllocMap {
    /* Head of the map. */
    struct AllocMapEntry*   rbh_root;
} AllocMap;

// =============================================================================
// Map API
// =============================================================================

/* Initializes allocation descriptors map.
 * Param:
 *  map - Allocation descriptors map to initialize.
 */
void allocmap_init(AllocMap* map);

/* Inserts new (or replaces existing) entry in the allocation descriptors map.
 * Insertion, or replacement is controlled by the value, passed to this routine
 * with 'replaced' parameter. If this parameter is NULL, insertion will fail if
 * a matching entry already exists in the map. If 'replaced' parameter is not
 * NULL, and a matching entry exists in the map, content of the existing entry
 * will be copied to the descriptor, addressed by 'replace' parameter, existing
 * entry will be removed from the map, and new entry will be inserted.
 * Param:
 *  map - Allocation descriptors map where to insert new, or replace existing
 *      entry.
 *  desc - Allocation descriptor to insert to the map.
 *  replaced - If not NULL, upon return from this routine contains descriptor
 *      that has been replaced in the map with the new entry. Note that if this
 *      routine returns with value other than RBT_MAP_RESULT_ENTRY_REPLACED,
 *      content of the 'replaced' buffer is not defined, as no replacement has
 *      actually occurred.
 * Return
 *  See RBTMapResult for the return codes.
 */
RBTMapResult allocmap_insert(AllocMap* map,
                             const MallocDescEx* desc,
                             MallocDescEx* replaced);

/* Finds an entry in the allocation descriptors map that matches the given
 * address.
 * Param:
 *  map - Allocation descriptors map where to search for an entry.
 *  address - Virtual address in the guest's user space to find matching
 *      entry for. Entry matches the address, if address is contained within
 *      allocated memory range (including guarding areas), as defined by the
 *      memory allocation descriptor for that entry.
 *  block_size - Size of the block, beginning with 'address'.
 * Return:
 *  Pointer to the allocation descriptor found in a map entry, or NULL if no
 *  matching entry has been found in the map.
 */
MallocDescEx* allocmap_find(const AllocMap* map,
                            target_ulong address,
                            uint32_t block_size);

/* Pulls (finds and removes) an entry from the allocation descriptors map that
 * matches the given address.
 * Param:
 *  map - Allocation descriptors map where to search for an entry.
 *  address - Virtual address in the guest's user space to find matching
 *      entry for. Entry matches the address, if address is contained within
 *      allocated memory range (including guarding areas), as defined by the
 *      memory allocation descriptor for that entry.
 *  pulled - Upon successful return contains allocation descriptor data pulled
 *      from the map.
 * Return:
 *  Zero if an allocation descriptor that matches the given address has
 *  been pulled, or 1 if no matching entry has been found in the map.
 */
int allocmap_pull(AllocMap* map, target_ulong address, MallocDescEx* pulled);

/* Pulls (removes) an entry from the head of the allocation descriptors map.
 * Param:
 *  map - Allocation descriptors map where to pull an entry from.
 *  pulled - Upon successful return contains allocation descriptor data pulled
 *      from the head of the map.
 * Return:
 *  Zero if an allocation descriptor has been pulled from the head of the map,
 *  or 1 if map is empty.
 */
int allocmap_pull_first(AllocMap* map, MallocDescEx* pulled);

/* Copies content of one memory allocation descriptors map to another.
 * Param:
 *  to - Map where to copy entries to.
 *  from - Map where to copy entries from.
 *  set_flags - Flags that should be set in the copied entry's 'flags' field.
 *  celar_flags - Flags that should be cleared in the copied entry's 'flags'
 *  field.
 * Return:
 *  Zero on success, or -1 on error.
 */
int allocmap_copy(AllocMap* to,
                  const AllocMap* from,
                  uint32_t set_flags,
                  uint32_t clear_flags);

/* Empties the map.
 * Param:
 *  map - Map to empty.
 * Return:
 *  Number of entries removed from the map.
 */
int allocmap_empty(AllocMap* map);

#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif  // QEMU_MEMCHECK_MEMCHECK_MALLOC_MAP_H
