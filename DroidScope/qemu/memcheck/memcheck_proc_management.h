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
 * Contains declarations of structures, routines, etc. related to process
 * management in memchecker framework.
 */

#ifndef QEMU_MEMCHECK_MEMCHECK_PROC_MANAGEMENT_H
#define QEMU_MEMCHECK_MEMCHECK_PROC_MANAGEMENT_H

#include "qemu-queue.h"
#include "memcheck_common.h"
#include "memcheck_malloc_map.h"
#include "memcheck_mmrange_map.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Process management structures
// =============================================================================

/* Describes a process that is monitored by memchecker framework. */
typedef struct ProcDesc {
    /* Map of memory blocks allocated in context of this process. */
    AllocMap                                    alloc_map;

    /* Map of memory mapped modules loaded in context of this process. */
    MMRangeMap                                  mmrange_map;

    /* Descriptor's entry in the global process list. */
    QLIST_ENTRY(ProcDesc)                        global_entry;

    /* List of threads running in context of this process. */
    QLIST_HEAD(threads, ThreadDesc)              threads;

    /* Path to the process' image file. */
    char*                                       image_path;

    /* Process id. */
    uint32_t                                    pid;

    /* Parent process id. */
    uint32_t                                    parent_pid;

    /* Misc. process flags. See PROC_FLAG_XXX */
    uint32_t                                    flags;
} ProcDesc;

/* Process is executing. */
#define PROC_FLAG_EXECUTING             0x00000001
/* Process is exiting. */
#define PROC_FLAG_EXITING               0x00000002
/* ProcDesc->image_path has been replaced during process execution. */
#define PROC_FLAG_IMAGE_PATH_REPLACED   0x00000004
/* libc.so instance has been initialized for this process. */
#define PROC_FLAG_LIBC_INITIALIZED      0x00000008

/* Entry in the thread's calling stack array. */
typedef struct ThreadCallStackEntry {
    /* Guest PC where call has been made. */
    target_ulong    call_address;
    /* Guest PC where call has been made, relative to the beginning of the
     * mapped module that contains call_address. */
    target_ulong    call_address_rel;
    /* Guest PC where call will return. */
    target_ulong    ret_address;
    /* Guest PC where call will return, relative to the beginning of the
     * mapped module that contains ret_address. */
    target_ulong    ret_address_rel;
    /* Path to the image file of the module containing call_address. */
    char*           module_path;
} ThreadCallStackEntry;

/* Describes a thread that is monitored by memchecker framework. */
typedef struct ThreadDesc {
    /* Descriptor's entry in the global thread list. */
    QLIST_ENTRY(ThreadDesc)  global_entry;

    /* Descriptor's entry in the process' thread list. */
    QLIST_ENTRY(ThreadDesc)  proc_entry;

    /* Descriptor of the process this thread belongs to. */
    ProcDesc*               process;

    /* Calling stack for this thread. */
    ThreadCallStackEntry*   call_stack;

    /* Number of entries in the call_stack array. */
    uint32_t                call_stack_count;

    /* Maximum number of entries that can fit into call_stack buffer. */
    uint32_t                call_stack_max;

    /* Thread id. */
    uint32_t                tid;
} ThreadDesc;

// =============================================================================
// Inlines
// =============================================================================

/* Checks if process has been forked, rather than created from a "fresh" PID.
 * Param:
 *  proc - Descriptor for the process to check.
 * Return:
 *  boolean: 1 if process has been forked, or 0 if it was
 *  created from a "fresh" PID.
 */
static inline int
procdesc_is_forked(const ProcDesc* proc)
{
    return proc->parent_pid != 0;
}

/* Checks if process is executing.
 * Param:
 *  proc - Descriptor for the process to check.
 * Return:
 *  boolean: 1 if process is executing, or 0 if it is not executing.
 */
static inline int
procdesc_is_executing(const ProcDesc* proc)
{
    return (proc->flags & PROC_FLAG_EXECUTING) != 0;
}

/* Checks if process is exiting.
 * Param:
 *  proc - Descriptor for the process to check.
 * Return:
 *  boolean: 1 if process is exiting, or 0 if it is still alive.
 */
static inline int
procdesc_is_exiting(const ProcDesc* proc)
{
    return (proc->flags & PROC_FLAG_EXITING) != 0;
}

/* Checks if process has initialized its libc.so instance.
 * Param:
 *  proc - Descriptor for the process to check.
 * Return:
 *  boolean: 1 if process has initialized its libc.so instance, or 0 otherwise.
 */
static inline int
procdesc_is_libc_initialized(const ProcDesc* proc)
{
    return (proc->flags & PROC_FLAG_LIBC_INITIALIZED) != 0;
}

/* Checks if process image path has been replaced.
 * Param:
 *  proc - Descriptor for the process to check.
 * Return:
 *  boolean: 1 if process image path has been replaced,
 *  or 0 if it was not replaced.
 */
static inline int
procdesc_is_image_path_replaced(const ProcDesc* proc)
{
    return (proc->flags & PROC_FLAG_IMAGE_PATH_REPLACED) != 0;
}

// =============================================================================
// Process management API
// =============================================================================

/* Gets thread descriptor for the current thread.
 * Return:
 *  Found thread descriptor, or NULL if thread descriptor has not been found.
 */
ThreadDesc* get_current_thread(void);

/* Initializes process management API. */
void memcheck_init_proc_management(void);

/* Gets process descriptor for the current process.
 * Return:
 *  Process descriptor for the current process, or NULL, if process descriptor
 *  has not been found.
 */
ProcDesc* get_current_process(void);

/* Finds process descriptor for a process id.
 * Param:
 *  pid - Process ID to look up process descriptor for.
 * Return:
 *  Process descriptor for the PID, or NULL, if process descriptor
 *  has not been found.
 */
ProcDesc* get_process_from_pid(uint32_t pid);

/* Inserts new (or replaces existing) entry in the allocation descriptors map
 * for the given process.
 * See allocmap_insert for more information on this routine, its parameters
 * and returning value.
 * Param:
 *  proc - Process descriptor where to add new allocation entry info.
 */
static inline RBTMapResult
procdesc_add_malloc(ProcDesc* proc,
                    const MallocDescEx* desc,
                    MallocDescEx* replaced)
{
    return allocmap_insert(&proc->alloc_map, desc, replaced);
}

/* Finds an entry in the allocation descriptors map for the given process,
 * matching given address range.
 * See allocmap_find for more information on this routine, its parameters
 * and returning value.
 * Param:
 *  proc - Process descriptor where to find an allocation entry.
 */
static inline MallocDescEx*
procdesc_find_malloc_for_range(ProcDesc* proc,
                               target_ulong address,
                               uint32_t block_size)
{
    return allocmap_find(&proc->alloc_map, address, block_size);
}

/* Finds an entry in the allocation descriptors map for the given process,
 * matching given address.
 * See allocmap_find for more information on this routine, its parameters
 * and returning value.
 * Param:
 *  proc - Process descriptor where to find an allocation entry.
 */
static inline MallocDescEx*
procdesc_find_malloc(ProcDesc* proc, target_ulong address)
{
    return procdesc_find_malloc_for_range(proc, address, 1);
}

/* Pulls (finds and removes) an entry from the allocation descriptors map for
 * the given process, matching given address.
 * See allocmap_pull for more information on this routine, its parameters
 * and returning value.
 * Param:
 *  proc - Process descriptor where to pull an allocation entry from.
 */
static inline int
procdesc_pull_malloc(ProcDesc* proc, target_ulong address, MallocDescEx* pulled)
{
    return allocmap_pull(&proc->alloc_map, address, pulled);
}

/* Empties allocation descriptors map for the process.
 * Param:
 *  proc - Process to empty allocation map for.
 * Return:
 *  Number of entries deleted from the allocation map.
 */
static inline int
procdesc_empty_alloc_map(ProcDesc* proc)
{
    return allocmap_empty(&proc->alloc_map);
}

/* Finds mmapping entry for the given address in the given process.
 * Param:
 *  proc - Descriptor of the process where to look for an entry.
 *  addr - Address in the guest space for which to find an entry.
 * Return:
 *  Mapped entry, or NULL if no mapping for teh given address exists in the
 *  process address space.
 */
static inline MMRangeDesc*
procdesc_find_mapentry(const ProcDesc* proc, target_ulong addr)
{
    return mmrangemap_find(&proc->mmrange_map, addr, addr + 1);
}

/* Gets module descriptor for the given address.
 * Param:
 *  proc - Descriptor of the process where to look for a module.
 *  addr - Address in the guest space for which to find a module descriptor.
 * Return:
 *  module descriptor for the module containing the given address, or NULL if no
 *  such module has been found in the process' map of mmaped modules.
 */
static inline const MMRangeDesc*
procdesc_get_range_desc(const ProcDesc* proc, target_ulong addr)
{
    return procdesc_find_mapentry(proc, addr);
}

/* Gets name of the module mmaped in context of the given process for the
 * given address.
 * Param:
 *  proc - Descriptor of the process where to look for a module.
 *  addr - Address in the guest space for which to find a module.
 * Return:
 *  Image path to the module containing the given address, or NULL if no such
 *  module has been found in the process' map of mmaped modules.
 */
static inline const char*
procdesc_get_module_path(const ProcDesc* proc, target_ulong addr)
{
    MMRangeDesc* rdesc = procdesc_find_mapentry(proc, addr);
    return rdesc != NULL ? rdesc->path : NULL;
}

#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif  // QEMU_MEMCHECK_MEMCHECK_PROC_MANAGEMENT_H
