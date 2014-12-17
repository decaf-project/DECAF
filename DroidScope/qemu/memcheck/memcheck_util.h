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
 * Contains declarations of utility routines for memchecker framework.
 */

#ifndef QEMU_MEMCHECK_MEMCHECK_UTIL_H
#define QEMU_MEMCHECK_MEMCHECK_UTIL_H

#include "memcheck_common.h"
#include "elff/elff_api.h"
#include "exec.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Transfering data between guest and emulator address spaces.
// =============================================================================

/* Copies buffer residing in the guest's virtual address space to a buffer
 * in the emulator's address space.
 * Param:
 *  guest_address - Address of the bufer in guest's virtual address space.
 *  qemu_address - Address of the bufer in the emulator's address space.
 *  buffer_size - Byte size of the guest's buffer.
 */
void memcheck_get_guest_buffer(void* qemu_address,
                               target_ulong guest_address,
                               size_t buffer_size);

/* Copies buffer residing in the emulator's address space to a buffer in the
 * guest's virtual address space.
 * Param:
 *  qemu_address - Address of the bufer in the emulator's address space.
 *  guest_address - Address of the bufer in guest's virtual address space.
 *  buffer_size - Byte size of the emualtor's buffer.
 */
void memcheck_set_guest_buffer(target_ulong guest_address,
                               const void* qemu_address,
                               size_t buffer_size);

/* Copies zero-terminated string residing in the guest's virtual address space
 * to a string buffer in emulator's address space.
 * Param:
 *  qemu_str - Address of the string bufer in the emulator's address space.
 *  guest_str - Address of the string in guest's virtual address space.
 *  qemu_buffer_size - Size of the emulator's string buffer.
 * Return
 *  Length of the string that has been copied.
 */
size_t memcheck_get_guest_string(char* qemu_str,
                                 target_ulong guest_str,
                                 size_t qemu_buffer_size);

/* Copies zero-terminated string residing in the guest's kernel address space
 * to a string buffer in emulator's address space.
 * Param:
 *  qemu_str - Address of the string bufer in the emulator's address space.
 *  guest_str - Address of the string in guest's kernel address space.
 *  qemu_buffer_size - Size of the emulator's string buffer.
 * Return
 *  Length of the string that has been copied.
 */
size_t memcheck_get_guest_kernel_string(char* qemu_str,
                                        target_ulong guest_str,
                                        size_t qemu_buffer_size);

// =============================================================================
// Helpers for transfering memory allocation information.
// =============================================================================

/* Copies memory allocation descriptor from the guest's address space to the
 * emulator's memory.
 * Param:
 *  qemu_address - Descriptor address in the emulator's address space where to
 *      copy descriptor.
 *  guest_address - Descriptor address in the guest's address space.
 */
static inline void
memcheck_get_malloc_descriptor(MallocDesc* qemu_address,
                               target_ulong guest_address)
{
    memcheck_get_guest_buffer(qemu_address, guest_address, sizeof(MallocDesc));
}

/* Copies memory allocation descriptor from the emulator's memory to the guest's
 * address space.
 * Param:
 *  guest_address - Descriptor address in the guest's address space.
 *  qemu_address - Descriptor address in the emulator's address space where to
 *  copy descriptor.
 */
static inline void
memcheck_set_malloc_descriptor(target_ulong guest_address,
                               const MallocDesc* qemu_address)
{
    memcheck_set_guest_buffer(guest_address, qemu_address, sizeof(MallocDesc));
}

/* Copies memory free descriptor from the guest's address space to the
 * emulator's memory.
 * Param:
 *  qemu_address - Descriptor address in the emulator's address space where to
 *      copy descriptor.
 *  guest_address - Descriptor address in the guest's address space.
 */
static inline void
memcheck_get_free_descriptor(MallocFree* qemu_address,
                             target_ulong guest_address)
{
    memcheck_get_guest_buffer(qemu_address, guest_address, sizeof(MallocFree));
}

/* Copies memory allocation query descriptor from the guest's address space to
 * the emulator's memory.
 * Param:
 *  guest_address - Descriptor address in the guest's address space.
 *  qemu_address - Descriptor address in the emulator's address space where to
 *      copy descriptor.
 */
static inline void
memcheck_get_query_descriptor(MallocDescQuery* qemu_address,
                              target_ulong guest_address)
{
    memcheck_get_guest_buffer(qemu_address, guest_address,
                              sizeof(MallocDescQuery));
}

/* Fails allocation request (TRACE_DEV_REG_MALLOC event).
 * Allocation request failure is reported by zeroing 'libc_pid' filed in the
 * allocation descriptor in the guest's address space.
 * Param:
 *  guest_address - Allocation descriptor address in the guest's address space,
 *      where to record failure.
 */
void memcheck_fail_alloc(target_ulong guest_address);

/* Fails free request (TRACE_DEV_REG_FREE_PTR event).
 * Free request failure is reported by zeroing 'libc_pid' filed in the free
 * descriptor in the guest's address space.
 * Param:
 *  guest_address - Free descriptor address in the guest's address space, where
 *      to record failure.
 */
void memcheck_fail_free(target_ulong guest_address);

/* Fails memory allocation query request (TRACE_DEV_REG_QUERY_MALLOC event).
 * Query request failure is reported by zeroing 'libc_pid' filed in the query
 * descriptor in the guest's address space.
 * Param:
 *  guest_address - Query descriptor address in the guest's address space, where
 *      to record failure.
 */
void memcheck_fail_query(target_ulong guest_address);

// =============================================================================
// Misc. utility routines.
// =============================================================================

/* Converts PC address in the translated block to a corresponded PC address in
 * the guest address space.
 * Param:
 *  tb_pc - PC address in the translated block.
 * Return:
 *  Corresponded PC address in the guest address space on success, or NULL if
 *  conversion has failed.
 */
static inline target_ulong
memcheck_tpc_to_gpc(target_ulong tb_pc)
{
    const TranslationBlock* tb = tb_find_pc(tb_pc);
    return tb != NULL ? tb_search_guest_pc_from_tb_pc(tb, tb_pc) : 0;
}

/* Invalidates TLB table pages that contain given memory range.
 * This routine is called after new entry is inserted into allocation map, so
 * every access to the allocated block will cause __ld/__stx_mmu to be called.
 * Param:
 *  start - Beginning of the allocated block to invalidate pages for.
 *  end - End of (past one byte after) the allocated block to invalidate pages
 *      for.
 */
void invalidate_tlb_cache(target_ulong start, target_ulong end);

/* Gets routine, file path and line number information for a PC address in the
 * given module.
 * Param:
 *  abs_pc - PC address.
 *  rdesc - Mapped memory range descriptor for the module containing abs_pc.
 *  info - Upon successful return will contain routine, file path and line
 *      information for the given PC address in the given module.
 *      NOTE: Pathnames, saved into this structure are contained in mapped
 *      sections of the symbols file for the module addressed by module_path.
 *      Thus, pathnames are accessible only while elff_handle returned from this
 *      routine remains opened.
 *      NOTE: each successful call to this routine requires the caller to call
 *      elff_free_pc_address_info for Elf_AddressInfo structure.
 *  elff_handle - Upon successful return will contain a handle to the ELFF API
 *      that wraps symbols file for the module, addressed by module_path. The
 *      handle must remain opened for as long as pathnames in the info structure
 *      are accessed, and must be eventually closed via call to elff_close.
 * Return:
 *  0 on success, 1, if symbols file for the module has not been found, or -1 on
 *  other failures. If a failure is returned from this routine content of info
 *  and elff_handle parameters is undefined.
 */
int memcheck_get_address_info(target_ulong abs_pc,
                              const MMRangeDesc* rdesc,
                              Elf_AddressInfo* info,
                              ELFF_HANDLE* elff_handle);

/* Dumps content of an allocation descriptor to stdout.
 * Param desc - Allocation descriptor to dump.
 * print_flags - If 1, flags field of the descriptor will be dumped to stdout.
 *      If 0, flags filed will not be dumped.
 * print_proc_info - If 1, allocator's process information for the descriptor
 *      will be dumped to stdout. If 0, allocator's process information will
 *      not be dumped.
 */
void memcheck_dump_malloc_desc(const MallocDescEx* desc,
                               int print_flags,
                               int print_proc_info);

#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif  // QEMU_MEMCHECK_MEMCHECK_UTIL_H
